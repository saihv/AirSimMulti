#include "AirSim.h"
#include "SimModeWorldMultiRotor.h"
#include "AirBlueprintLib.h"
#include "controllers/DroneControllerBase.hpp"
#include "physics/PhysicsBody.hpp"
#include <memory>
#include "FlyingPawn.h"

void ASimModeWorldMultiRotor::BeginPlay()
{
    Super::BeginPlay();

    if (fpv_vehicle_connector_.size() != 0) {
        //create its control server
        try {
			for (auto connector : fpv_vehicle_connector_)
			{
				connector->startApiServer();
			}
        }
        catch (std::exception& ex) {
            UAirBlueprintLib::LogMessage("Cannot start RpcLib Server",  ex.what(), LogDebugLevel::Failure);
        }
    }
}

void ASimModeWorldMultiRotor::Tick(float DeltaSeconds)
{
    if (fpv_vehicle_connector_[0] != nullptr && fpv_vehicle_connector_[0]->isApiServerStarted() && getVehicleCount() > 0) {

        using namespace msr::airlib;
        
		auto controller = static_cast<DroneControllerBase*>(fpv_vehicle_connector_[0]->getController());
        auto camera_type = controller->getImageTypeForCamera(0);
        if (camera_type != DroneControllerBase::ImageType::None) { 
            if (CameraDirector != nullptr) {
                APIPCamera* camera = pawn2->getFpvCamera();
                EPIPCameraType pip_type;
                if (camera != nullptr) {
                    //TODO: merge these two different types?
                    switch (camera_type) {
                    case DroneControllerBase::ImageType::Scene:
                        pip_type = EPIPCameraType::PIP_CAMERA_TYPE_SCENE; break;
                    case DroneControllerBase::ImageType::Depth:
                        pip_type = EPIPCameraType::PIP_CAMERA_TYPE_DEPTH; break;
                    case DroneControllerBase::ImageType::Segmentation:
                        pip_type = EPIPCameraType::PIP_CAMERA_TYPE_SEG; break;
                    default:
                        pip_type = EPIPCameraType::PIP_CAMERA_TYPE_NONE;
                    }
                    float width, height;
                    image_.Empty();
                    camera->getScreenshot(pip_type, image_, width, height);
                    controller->setImageForCamera(0, camera_type, std::vector<msr::airlib::uint8_t>(image_.GetData(), image_.GetData() + image_.Num()));
                }
            }
        }
		
		if (isRecording() && record_file.is_open()) {
			if (!isLoggingStarted)
			{
				FString imagePathPrefix = common_utils::FileSystem::getLogFileNamePath("img_", "", "", false).c_str();
				FCameraLogger::ThreadInit(imagePathPrefix, this);
				isLoggingStarted = true;
			}
		}

		if (!isRecording() && isLoggingStarted)
		{
			FCameraLogger::Shutdown();
			isLoggingStarted = false;
		}
    }

    Super::Tick(DeltaSeconds);
}

void ASimModeWorldMultiRotor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (auto connector : fpv_vehicle_connector_) {
		if (connector != nullptr)
			connector->stopApiServer();
	}
	FCameraLogger::Shutdown();
    Super::EndPlay(EndPlayReason);
}

bool ASimModeWorldMultiRotor::checkConnection()
{
    return true;
}

void ASimModeWorldMultiRotor::createVehicles(std::vector<VehiclePtr>& vehicles)
{
    if (!checkConnection())
        return;

    //get FPV drone
    AActor* fpv_pawn = nullptr;
    if (CameraDirector != nullptr) {
        fpv_pawn = CameraDirector->TargetPawn;
    }

    //detect vehicles in the project and add them in simulation
    TArray<AActor*> pawns;
    UAirBlueprintLib::FindAllActor<AFlyingPawn>(this, pawns);
    for (AActor* pawn : pawns) {
        auto vehicle = createVehicle(static_cast<AFlyingPawn*>(pawn));

        if (vehicle != nullptr) {
            vehicles.push_back(vehicle);
			
			if ((*((AFlyingPawn*)(pawn))).VehicleName.Equals(TEXT("Quad1"), ESearchCase::CaseSensitive))
			{
				fpv_vehicle_connector_.push_back(vehicle);
				pawn1 = static_cast<AFlyingPawn*>(pawn);
			}
			 
			else if ((*((AFlyingPawn*)(pawn))).VehicleName.Equals(TEXT("Quad2"), ESearchCase::CaseSensitive))
			{
				fpv_vehicle_connector_.push_back(vehicle);
				pawn2 = static_cast<AFlyingPawn*>(pawn);
			}
        }
        //else we don't have vehicle for this pawn
    }
}

ASimModeWorldBase::VehiclePtr ASimModeWorldMultiRotor::createVehicle(AFlyingPawn* pawn)
{
    auto vehicle = std::make_shared<MultiRotorConnector>();
    vehicle->initialize(pawn, MultiRotorConnector::ConfigType::Pixhawk);
    return std::static_pointer_cast<VehicleConnectorBase>(vehicle);
}


