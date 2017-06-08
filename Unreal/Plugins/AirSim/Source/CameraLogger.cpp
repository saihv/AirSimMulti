#include <AirSim.h>
#include <CameraLogger.h>
#include "TaskGraphInterfaces.h"

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
		cam = GameThread->game_pawns[0]->getFpvCamera();
		capture = cam->getCaptureComponent(EPIPCameraType::PIP_CAMERA_TYPE_SCENE, true);
		break;

	case 2:
		cam = GameThread->game_pawns[1]->getFpvCamera();
		capture = cam->getCaptureComponent(EPIPCameraType::PIP_CAMERA_TYPE_DEPTH, true);
		break;

	case 3:
		cam = GameThread->game_pawns[2]->getFpvCamera();
		capture = cam->getCaptureComponent(EPIPCameraType::PIP_CAMERA_TYPE_SEG, true);
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
					FReadSurfaceDataFlags()
				};

				// bReadPixelsStarted = true;

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
		ReadPixelsNonBlocking(imageColor, currentId);

		// Declare task graph 'RenderStatus' in order to check on the status of the queued up render command
		DECLARE_CYCLE_STAT(TEXT("FNullGraphTask.CheckRenderStatus"), STAT_FNullGraphTask_CheckRenderStatus, STATGROUP_TaskGraphTasks);
		RenderStatus = TGraphTask<FNullGraphTask>::CreateTask(NULL).ConstructAndDispatchWhenReady(GET_STATID(STAT_FNullGraphTask_CheckRenderStatus), ENamedThreads::RenderThread);
		// Now queue a dependent task to run when the rendering operation is complete.
		CompletionStatus = FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
			FSimpleDelegateGraphTask::FDelegate::CreateLambda([=]()
		{
			if (StopTaskCounter.GetValue() == 0) {
				FCameraLogger::SaveImage(currentId);
			}
		}),
			TStatId(),
			RenderStatus
			);
					// wait for both tasks to complete.
		FTaskGraphInterface::Get().WaitUntilTaskCompletes(CompletionStatus);
	}

	return 0;
}

void FCameraLogger::SaveImage(int id)
{
	bool complete = (RenderStatus.GetReference() && RenderStatus->IsComplete());

	if (imageColor.Num() > 0 && complete) {
		RenderStatus = NULL;

		TArray<uint8> compressedPng;
		FIntPoint dest(width, height);
		FImageUtils::CompressImageArray(dest.X, dest.Y, imageColor, compressedPng);

		std::string vehicleId;

		switch (id) {
		case 1:
			vehicleId = "_Quad1_";
			break;
		case 2:
			vehicleId = "_Quad2_";
			break;
		case 3:
			vehicleId = "_Quad3_";
			break;
		}

		FString filePath = imagePath + FString(vehicleId.c_str()) + FString::FromInt(imagesSaved) + ".png";
		bool imageSavedOk = FFileHelper::SaveArrayToFile(compressedPng, *filePath);

		// If render command is complete, save image along with position and orientation

		if (!imageSavedOk)
			UAirBlueprintLib::LogMessage(TEXT("FAILED to save screenshot to:"), filePath, LogDebugLevel::Failure);
		else {
			auto physics_body = static_cast<msr::airlib::PhysicsBody*>(GameThread->fpv_vehicle_connector_[id-1]->getPhysicsBody());
			auto kinematics = physics_body->getKinematics();

			GameThread->record_file << msr::airlib::Utils::getTimeSinceEpochMillis() << "\t";
			GameThread->record_file << kinematics.pose.position.x() << "\t" << kinematics.pose.position.y() << "\t" << kinematics.pose.position.z() << "\t";
			GameThread->record_file << kinematics.pose.orientation.w() << "\t" << kinematics.pose.orientation.x() << "\t" << kinematics.pose.orientation.y() << "\t" << kinematics.pose.orientation.z() << "\t";
			GameThread->record_file << "\n";

			UAirBlueprintLib::LogMessage(TEXT("Screenshot saved to:"), filePath, LogDebugLevel::Success);
			

			if (currentId < 3)
				currentId++;
			else if (currentId == 3)
			{
				currentId = 1;
				imagesSaved++;
			}
		}	
	}
}

void FCameraLogger::Stop()
{
	StopTaskCounter.Increment();
	Thread->WaitForCompletion();
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