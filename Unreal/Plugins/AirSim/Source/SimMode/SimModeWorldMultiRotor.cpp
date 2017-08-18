#include "SimModeWorldMultiRotor.h"
#include "ConstructorHelpers.h"
#include "AirBlueprintLib.h"
#include "controllers/DroneControllerBase.hpp"
#include "physics/PhysicsBody.hpp"
#include <memory>
#include "FlyingPawn.h"
#include "Logging/MessageLog.h"
#include "vehicles/MultiRotorParamsFactory.hpp"


ASimModeWorldMultiRotor::ASimModeWorldMultiRotor()
{
    static ConstructorHelpers::FClassFinder<APIPCamera> external_camera_class(TEXT("Blueprint'/AirSim/Blueprints/BP_PIPCamera'"));
    external_camera_class_ = external_camera_class.Succeeded() ? external_camera_class.Class : nullptr;
    static ConstructorHelpers::FClassFinder<ACameraDirector> camera_director_class(TEXT("Blueprint'/AirSim/Blueprints/BP_CameraDirector'"));
    camera_director_class_ = camera_director_class.Succeeded() ? camera_director_class.Class : nullptr;
    static ConstructorHelpers::FClassFinder<AVehiclePawnBase> vehicle_pawn_class(TEXT("Blueprint'/AirSim/Blueprints/BP_FlyingPawn'"));
    vehicle_pawn_class_ = vehicle_pawn_class.Succeeded() ? vehicle_pawn_class.Class : nullptr;
    current_camera_director = 0;
}

void ASimModeWorldMultiRotor::BeginPlay()
{
    Super::BeginPlay();
    setupInputBindings();
    if (fpv_vehicle_connector_.Num() > 0){
        for (std::shared_ptr<VehicleConnectorBase> vehicule_connector : fpv_vehicle_connector_){
            try{
                vehicule_connector->startApiServer();
            } catch(std::exception& ex){
                UAirBlueprintLib::LogMessageString("Cannot start RpcLib Server",  ex.what(), LogDebugLevel::Failure);
            }
        }
    }
}

AVehiclePawnBase* ASimModeWorldMultiRotor::getFpvVehiclePawn(int indx)
{
    return fpv_vehicle_pawn_[indx<fpv_vehicle_pawn_.Num() ? indx : (fpv_vehicle_pawn_.Num()-1)];
}

void ASimModeWorldMultiRotor::setupVehiclesAndCamera()
{
    TArray<AActor*> pawns;
    UAirBlueprintLib::FindAllActor<AVehiclePawnBase>(this, pawns);
    if( pawns.Num() == 0){
        APlayerController* controller = this->GetWorld()->GetFirstPlayerController();
        FTransform actor_transform = controller->GetActorTransform();
        //put camera little bit above vehicle
        FTransform camera_transform(actor_transform.GetLocation() + FVector(-300, 0, 200));

        //External camera
        APIPCamera* external_camera;

        //create director
        FActorSpawnParameters camera_spawn_params;
        camera_spawn_params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        CameraDirector = this->GetWorld()->SpawnActor<ACameraDirector>(camera_director_class_, camera_transform, camera_spawn_params);
        //spawned_actors_.Add(CameraDirector);
        //ind_camera_directors.Add(spawned_actors_.Num() - 1);

        //create external camera required for the director
        external_camera = this->GetWorld()->SpawnActor<APIPCamera>(external_camera_class_, camera_transform, camera_spawn_params);
        spawned_actors_.Add(external_camera);

        //create one pawn vehicule
        FActorSpawnParameters pawn_spawn_params;
        pawn_spawn_params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

        AVehiclePawnBase* vehicule_pawn = this->GetWorld()->SpawnActor<AVehiclePawnBase>(vehicle_pawn_class_, actor_transform, pawn_spawn_params);
        fpv_vehicle_pawn_.Add(vehicule_pawn);

        if(enable_collision_passthrough)
            vehicule_pawn->EnablePassthroughOnCollisons = true;
        vehicule_pawn->initializeForBeginPlay();
        CameraDirector->initializeForBeginPlay(getInitialViewMode() , vehicule_pawn , external_camera);
    }else {
        for (AActor* actor : pawns ){
            FTransform actor_transform = actor->GetActorTransform();
            FTransform camera_transform(actor_transform.GetLocation() + FVector(-300, 0, 200));

            //External camera
            APIPCamera* external_camera;
            //ACameraDirector* camDirector;

            //create director
            FActorSpawnParameters camera_spawn_params;
            camera_spawn_params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
            //camDirector = this->GetWorld()->SpawnActor<ACameraDirector>(camera_director_class_, camera_transform, camera_spawn_params);
            //spawned_actors_.Add(camDirector);
            //ind_camera_directors.Add(spawned_actors_.Num() - 1);

            //create external camera required for the director
            external_camera = this->GetWorld()->SpawnActor<APIPCamera>(external_camera_class_, camera_transform, camera_spawn_params);
            spawned_actors_.Add(external_camera);

            //create one pawn vehicule
            FActorSpawnParameters pawn_spawn_params;
            pawn_spawn_params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

            AVehiclePawnBase* vehicule_pawn = static_cast< AVehiclePawnBase* >(actor);
            fpv_vehicle_pawn_.Add(vehicule_pawn);

            if(enable_collision_passthrough)
                vehicule_pawn->EnablePassthroughOnCollisons = true;
            vehicule_pawn->initializeForBeginPlay();
            //camDirector->initializeForBeginPlay(getInitialViewMode() , vehicule_pawn , external_camera);
        }
        FTransform actor_transform = fpv_vehicle_pawn_[current_camera_director]->GetActorTransform();
        FTransform camera_transform(actor_transform.GetLocation() + FVector(-300, 0, 200));
        FActorSpawnParameters camera_spawn_params;
        camera_spawn_params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        CameraDirector = this->GetWorld()->SpawnActor<ACameraDirector>(camera_director_class_, camera_transform, camera_spawn_params);
        CameraDirector->initializeForBeginPlay(getInitialViewMode() , fpv_vehicle_pawn_[current_camera_director] , static_cast< APIPCamera* >(spawned_actors_[current_camera_director]));
    }
}

void ASimModeWorldMultiRotor::Tick(float DeltaSeconds)
{
    int number_quad = fpv_vehicle_connector_.Num();
    if (number_quad >0 && fpv_vehicle_connector_[current_camera_director]->isApiServerStarted() && getVehicleCount() > 0) {

        if (isRecording() && getRecordingFile().isRecording()) {
            if (!isLoggingStarted)
            {
                FRecordingThread::ThreadInit(fpv_vehicle_connector_[current_camera_director]->getCamera(), & getRecordingFile(), 
                    static_cast<msr::airlib::PhysicsBody*>(fpv_vehicle_connector_[current_camera_director]->getPhysicsBody()), recording_settings);
                isLoggingStarted = true;
            }
        }

        if (!isRecording() && isLoggingStarted)
        {
            FRecordingThread::Shutdown();
            isLoggingStarted = false;
        }
    }

    Super::Tick(DeltaSeconds);
}

void ASimModeWorldMultiRotor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    for (std::shared_ptr<VehicleConnectorBase> vehicule_connector : fpv_vehicle_connector_){
        vehicule_connector->stopApiServer();
    }

    if (isLoggingStarted)
    {
        FRecordingThread::Shutdown();
        isLoggingStarted = false;
    }

    for (AActor* actor : spawned_actors_) {
        actor->Destroy();
    }

    for (AVehiclePawnBase* vehicule_pawn : fpv_vehicle_pawn_){
        vehicule_pawn->Destroy();
    }

    for (int i = 0 ; i < fpv_vehicle_connector_.Num() ; i++){
        fpv_vehicle_connector_[i] = nullptr;
    }

    if (CameraDirector != nullptr) {
        CameraDirector->Destroy();
        CameraDirector = nullptr;
    }

    spawned_actors_.Empty();
    fpv_vehicle_connector_.Empty();
    fpv_vehicle_pawn_.Empty();
    vehicle_params_.Empty();
    current_camera_director = 0;

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

    //find vehicles and cameras available in environment
    //if none available then we will create one
    setupVehiclesAndCamera();

    //detect vehicles in the project and create connector for it
    TArray<AActor*> pawns;
    UAirBlueprintLib::FindAllActor<AFlyingPawn>(this, pawns);
    for (AActor* pawn : pawns) {
        auto vehicle = createVehicle(static_cast<AFlyingPawn*>(pawn));
        if (vehicle != nullptr) {
            vehicles.push_back(vehicle);
            fpv_vehicle_connector_.Add(vehicle);
        }
        //else we don't have vehicle for this pawn
    }
}

ASimModeWorldBase::VehiclePtr ASimModeWorldMultiRotor::createVehicle(AFlyingPawn* pawn)
{
    std::unique_ptr<msr::airlib::MultiRotorParams> vehicle_params = MultiRotorParamsFactory::createConfig(pawn->getVehicleName());
    vehicle_params_.Add(std::move(vehicle_params));
    auto vehicle = std::make_shared<MultiRotorConnector>();
    vehicle->initialize(pawn, vehicle_params_.Top().get(), enable_rpc, api_server_address, manual_pose_controller);
    return std::static_pointer_cast<VehicleConnectorBase>(vehicle);
}

void ASimModeWorldMultiRotor::inputEventPlayerCamera(){
    current_camera_director = (current_camera_director + 1) % (fpv_vehicle_pawn_.Num());
    FTransform actor_transform = fpv_vehicle_pawn_[current_camera_director]->GetActorTransform();
    FTransform camera_transform(actor_transform.GetLocation() + FVector(-300, 0, 200));
    CameraDirector->SetActorTransform(camera_transform,false);
    CameraDirector->setCameras(static_cast< APIPCamera* >(spawned_actors_[current_camera_director]), fpv_vehicle_pawn_[current_camera_director]);
}

void ASimModeWorldMultiRotor::setupInputBindings()
{

    Super::setupInputBindings();
    UAirBlueprintLib::EnableInput(this);
    UAirBlueprintLib::BindActionToKey("InputEventToggleCameraPlayer", EKeys::L, this , &ASimModeWorldMultiRotor::inputEventPlayerCamera);

}

