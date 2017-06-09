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
				game_pawns.push_back(static_cast<AFlyingPawn*>(pawn));
			}
			 
			else if ((*((AFlyingPawn*)(pawn))).VehicleName.Equals(TEXT("Quad2"), ESearchCase::CaseSensitive))
			{
				fpv_vehicle_connector_.push_back(vehicle);
				game_pawns.push_back(static_cast<AFlyingPawn*>(pawn));
			}

			else if ((*((AFlyingPawn*)(pawn))).VehicleName.Equals(TEXT("Quad3"), ESearchCase::CaseSensitive))
			{
				fpv_vehicle_connector_.push_back(vehicle);
				game_pawns.push_back(static_cast<AFlyingPawn*>(pawn));
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


