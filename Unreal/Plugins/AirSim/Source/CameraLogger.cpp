#include <AirSim.h>
#include <CameraLogger.h>

FCameraLogger* FCameraLogger::Runnable = NULL;

FCameraLogger::FCameraLogger(FString path, ASimModeWorldMultiRotor* AirSim)
	: GameThread(AirSim)
	, StopTaskCounter(0)
{
	imagePath = path;
	Thread = FRunnableThread::Create(this, TEXT("FCameraLogger"), 0, TPri_BelowNormal); 
}

FCameraLogger::~FCameraLogger()
{
	delete Thread;
	Thread = NULL;
}

bool FCameraLogger::Init()
{
	if (GameThread)
	{
		UAirBlueprintLib::LogMessage(TEXT("Initiated camera logging thread"), TEXT(""), LogDebugLevel::Success);
	}
	return true;
}

void FCameraLogger::ReadPixelsNonBlocking(TArray<FColor>& bmp, unsigned int id)
{
	APIPCamera* cam;
	USceneCaptureComponent2D* capture;
	
	switch (id)
	{
	case 1:
		cam = GameThread->pawn1->getFpvCamera();
		capture = cam->getCaptureComponent(EPIPCameraType::PIP_CAMERA_TYPE_SEG, true);
		break;

	case 2:
		cam = GameThread->pawn2->getFpvCamera();
		capture = cam->getCaptureComponent(EPIPCameraType::PIP_CAMERA_TYPE_DEPTH, true);
		break;

	default:
		capture = nullptr;
		break;
	}
	
	if (capture != nullptr) {
		if (capture->TextureTarget != nullptr) {
			FTextureRenderTargetResource* RenderResource = capture->TextureTarget->GetRenderTargetResource();
			if (RenderResource != nullptr) {
				width = capture->TextureTarget->GetSurfaceWidth();
				height = capture->TextureTarget->GetSurfaceHeight();
				// Read the render target surface data back.	
				struct FReadSurfaceContext
				{
					FRenderTarget* SrcRenderTarget;
					TArray<FColor>* OutData;
					FIntRect Rect;
					FReadSurfaceDataFlags Flags;
				};

				bmp.Reset();
				FReadSurfaceContext ReadSurfaceContext =
				{
					RenderResource,
					&bmp,
					FIntRect(0, 0, RenderResource->GetSizeXY().X, RenderResource->GetSizeXY().Y),
					FReadSurfaceDataFlags(RCM_UNorm, CubeFace_MAX)
				};

				bReadPixelsStarted = true;

				ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
					ReadSurfaceCommand,
					FReadSurfaceContext, Context, ReadSurfaceContext,
					{
						RHICmdList.ReadSurfaceData(
							Context.SrcRenderTarget->GetRenderTargetTexture(),
							Context.Rect,
							*Context.OutData,
							Context.Flags
						);
					});
			}
		}
	}
}

uint32 FCameraLogger::Run()
{
	while (StopTaskCounter.GetValue() == 0)
	{
		if (!bReadPixelsStarted && currentId != 1)
		{
			currentId = 1;
			ReadPixelsNonBlocking(imageColor, currentId);

			DECLARE_CYCLE_STAT(TEXT("FNullGraphTask.CheckRenderStatus"), STAT_FNullGraphTask_CheckRenderStatus, STATGROUP_TaskGraphTasks);
			RenderStatus = TGraphTask<FNullGraphTask>::CreateTask(NULL).ConstructAndDispatchWhenReady(GET_STATID(STAT_FNullGraphTask_CheckRenderStatus), ENamedThreads::RenderThread);
		}				

		if (bReadPixelsStarted && currentId == 1 && (!RenderStatus.GetReference() || RenderStatus->IsComplete())) {
			bReadPixelsStarted = false;
			RenderStatus = NULL;

			TArray<uint8> compressedPng;
			FIntPoint dest(width, height);
			FImageUtils::CompressImageArray(dest.X, dest.Y, imageColor, compressedPng);
			FString filePath = imagePath + "_Quad1_" + FString::FromInt(shotNum) + ".png";
			bool imageSavedOk = FFileHelper::SaveArrayToFile(compressedPng, *filePath);

			if (!imageSavedOk)
				UAirBlueprintLib::LogMessage(TEXT("File save failed to:"), filePath, LogDebugLevel::Failure);
			else {
				auto physics_body = static_cast<msr::airlib::PhysicsBody*>(GameThread->fpv_vehicle_connector_->getPhysicsBody());
				auto kinematics = physics_body->getKinematics();

				GameThread->record_file << msr::airlib::Utils::getTimeSinceEpochMillis() << "\t";    
				GameThread->record_file << kinematics.pose.position.x() << "\t" << kinematics.pose.position.y() << "\t" << kinematics.pose.position.z() << "\t";
				GameThread->record_file << kinematics.pose.orientation.w() << "\t" << kinematics.pose.orientation.x() << "\t" << kinematics.pose.orientation.y() << "\t" << kinematics.pose.orientation.z() << "\t";
				GameThread->record_file << "\n";

				UAirBlueprintLib::LogMessage(TEXT("Screenshot saved to:"), filePath, LogDebugLevel::Success);
			}
		}		
		
		if (!bReadPixelsStarted && currentId != 2)
		{
			currentId = 2;
			ReadPixelsNonBlocking(imageColor, currentId);
			
			DECLARE_CYCLE_STAT(TEXT("FNullGraphTask.CheckRenderStatus"), STAT_FNullGraphTask_CheckRenderStatus, STATGROUP_TaskGraphTasks);
			RenderStatus = TGraphTask<FNullGraphTask>::CreateTask(NULL).ConstructAndDispatchWhenReady(GET_STATID(STAT_FNullGraphTask_CheckRenderStatus), ENamedThreads::RenderThread);
		}

		if (bReadPixelsStarted && currentId == 2 && (!RenderStatus.GetReference() || RenderStatus->IsComplete())) {
			bReadPixelsStarted = false;
			RenderStatus = NULL;

			TArray<uint8> compressedPng;
			FIntPoint dest(width, height);
			FImageUtils::CompressImageArray(dest.X, dest.Y, imageColor, compressedPng);
			FString filePath = imagePath + "_Quad2_" + FString::FromInt(shotNum) + ".png";
			bool imageSavedOk = FFileHelper::SaveArrayToFile(compressedPng, *filePath);

			if (!imageSavedOk)
				UAirBlueprintLib::LogMessage(TEXT("File save failed to:"), filePath, LogDebugLevel::Failure);
			else {
				auto physics_body = static_cast<msr::airlib::PhysicsBody*>(GameThread->fpv_vehicle_connector_->getPhysicsBody());
				auto kinematics = physics_body->getKinematics();

				GameThread->record_file << msr::airlib::Utils::getTimeSinceEpochMillis() << "\t";
				GameThread->record_file << kinematics.pose.position.x() << "\t" << kinematics.pose.position.y() << "\t" << kinematics.pose.position.z() << "\t";
				GameThread->record_file << kinematics.pose.orientation.w() << "\t" << kinematics.pose.orientation.x() << "\t" << kinematics.pose.orientation.y() << "\t" << kinematics.pose.orientation.z() << "\t";
				GameThread->record_file << "\n";

				UAirBlueprintLib::LogMessage(TEXT("Screenshot saved to:"), filePath, LogDebugLevel::Success);
			}
			shotNum++;
		}		

	}
	return 0;
}

void FCameraLogger::Stop()
{
	StopTaskCounter.Increment();
}

FCameraLogger* FCameraLogger::ThreadInit(FString path, ASimModeWorldMultiRotor* AirSim)
{
	if (!Runnable && FPlatformProcess::SupportsMultithreading())
	{
		Runnable = new FCameraLogger(path, AirSim);
	}
	return Runnable;
}

void FCameraLogger::EnsureCompletion()
{
	Stop();
	Thread->WaitForCompletion();
	UAirBlueprintLib::LogMessage(TEXT("Stopped camera logging thread"), TEXT(""), LogDebugLevel::Success);
}

void FCameraLogger::Shutdown()
{
	if (Runnable)
	{
		Runnable->EnsureCompletion();
		delete Runnable;
		Runnable = NULL;
	}
}