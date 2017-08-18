#include "RecordingThread.h"
#include "HAL/RunnableThread.h"
#include <thread>
#include <mutex>
#include "TaskGraphInterfaces.h"
#include "RenderRequest.h"
#include "PIPCamera.h"

#include "ImageUtils.h"
#include "Engine/TextureRenderTarget2D.h"

FRecordingThread* FRecordingThread::instance_ = NULL;

FRecordingThread::FRecordingThread(ASimModeWorldMultiRotor* AirSim)
    : stop_task_counter_(0), camera_(nullptr), recording_file_(nullptr), fpv_physics_body_(nullptr), is_ready_(false)
{
    thread_ = FRunnableThread::Create(this, TEXT("FRecordingThread"), 0, TPri_BelowNormal); // Windows default, possible to specify more priority
}


FRecordingThread* FRecordingThread::ThreadInit(ASimModeWorldMultiRotor* AirSim, msr::airlib::VehicleCameraBase* camera, RecordingFile* recording_file, const msr::airlib::PhysicsBody* fpv_physics_body, const RecordingSettings& settings)
{
    if (!instance_ && FPlatformProcess::SupportsMultithreading())
    {
        instance_ = new FRecordingThread(AirSim);
        instance_->camera_ = camera;
        instance_->recording_file_ = recording_file;
        instance_->fpv_physics_body_ = fpv_physics_body;
        instance_->settings_ = settings;

        instance_->last_screenshot_on_ = 0;
        instance_->last_pose_ = msr::airlib::Pose();

        instance_->is_ready_ = true;
    }
    return instance_;
}

FRecordingThread::~FRecordingThread()
{
    delete thread_;
    thread_ = NULL;
}

bool FRecordingThread::Init()
{
    if (camera_ && recording_file_)
    {
        UAirBlueprintLib::LogMessage(TEXT("Initiated recording thread"), TEXT(""), LogDebugLevel::Success);
    }
    return true;
}

void FRecordingThread::ReadPixelsNonBlocking(TArray<FColor>& image)
{
	auto capture = camera_->getCaptureComponent(msr::airlib::VehicleCameraBase::ImageType_::Scene);

	if (capture != nullptr) {
		if (capture->TextureTarget != nullptr) {
			auto RenderResource = capture->TextureTarget->GetRenderTargetResource();
			if (RenderResource != nullptr) {
				/*
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

				image.Reset();
				FReadSurfaceContext ReadSurfaceContext =
				{
					RenderResource,
					&image,
					FIntRect(0, 0, RenderResource->GetSizeXY().X, RenderResource->GetSizeXY().Y),
					FReadSurfaceDataFlags(RCM_UNorm, CubeFace_MAX)
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
			*/}
		}
	}
}

void FRecordingThread::saveImage()
{
	bool complete = (RenderStatus.GetReference() && RenderStatus->IsComplete());

	if (imageColor.Num() > 0 && complete) {
		RenderStatus = NULL;

		TArray<uint8> compressedPng;
		FIntPoint dest(width, height);
		// FImageUtils::CompressImageArray(dest.X, dest.Y, imageColor, compressedPng);
		// recording_file_->appendRecord(compressedPng, fpv_physics_body_);
	}
}

uint32 FRecordingThread::Run()
{
	while (stop_task_counter_.GetValue() == 0)
	{
		//make sire all vars are set up
		if (is_ready_) {
			//bool interval_elapsed = msr::airlib::ClockFactory::get()->elapsedSince(last_screenshot_on_) > settings_.record_interval;
			//bool is_pose_unequal = fpv_physics_body_ && last_pose_ != fpv_physics_body_->getKinematics().pose;
			//if (interval_elapsed && (!settings_.record_on_move || is_pose_unequal))
			//{
				//last_screenshot_on_ = msr::airlib::ClockFactory::get()->nowNanos();
				//last_pose_ = fpv_physics_body_->getKinematics().pose;

				ReadPixelsNonBlocking(imageColor);
				DECLARE_CYCLE_STAT(TEXT("FNullGraphTask.CheckRenderStatus"), STAT_FNullGraphTask_CheckRenderStatus, STATGROUP_TaskGraphTasks);
				RenderStatus = TGraphTask<FNullGraphTask>::CreateTask(NULL).ConstructAndDispatchWhenReady(GET_STATID(STAT_FNullGraphTask_CheckRenderStatus), ENamedThreads::RenderThread);
				// Now queue a dependent task to run when the rendering operation is complete.
				CompletionStatus = FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
					FSimpleDelegateGraphTask::FDelegate::CreateLambda([=]()
				{
					if (stop_task_counter_.GetValue() == 0) {
						FRecordingThread::saveImage();
					}
				}),
					TStatId(),
					RenderStatus
					);
				// wait for both tasks to complete.
				FTaskGraphInterface::Get().WaitUntilTaskCompletes(CompletionStatus);

				//recording_file_->appendRecord(image_data, fpv_physics_body_);
			//}
		}
	}
    return 0;
}


void FRecordingThread::Stop()
{
    stop_task_counter_.Increment();
}


void FRecordingThread::EnsureCompletion()
{
    Stop();
    thread_->WaitForCompletion();
    UAirBlueprintLib::LogMessage(TEXT("Stopped recording thread"), TEXT(""), LogDebugLevel::Success);
}

void FRecordingThread::Shutdown()
{
    if (instance_)
    {
        instance_->EnsureCompletion();
        delete instance_;
        instance_ = NULL;
    }
}