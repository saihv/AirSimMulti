#pragma once

#include "CoreMinimal.h"
#include "AirBlueprintLib.h"
#include "HAL/Runnable.h"
#include "VehicleCameraConnector.h"
#include "Recording/RecordingFile.h"
#include "physics/PhysicsBody.hpp"
#include "common/ClockFactory.hpp"
#include "SimMode/SimModeWorldMultiRotor.h"

class FRecordingThread : public FRunnable
{
	ASimModeWorldMultiRotor* GameThread;

	TArray<FColor> imageColor;
	float width = 1280;
	float height = 720;
	FRenderCommandFence ReadPixelFence;
	
public:
    FRecordingThread(ASimModeWorldMultiRotor* GameThread);
    virtual ~FRecordingThread();
    static FRecordingThread* ThreadInit(ASimModeWorldMultiRotor* GameThread, msr::airlib::VehicleCameraBase* camera, RecordingFile* recording_file, const msr::airlib::PhysicsBody* fpv_physics_body, const RecordingSettings& settings);
    static void Shutdown();

private:
    virtual bool Init();
    virtual uint32 Run();
    virtual void Stop();

    void EnsureCompletion();

private:

	void ReadPixelsNonBlocking(TArray<FColor>& bmp);
    FRenderCommandFence read_pixel_fence_;
	FThreadSafeCounter stop_task_counter_;

    msr::airlib::VehicleCameraBase* camera_;
    RecordingFile* recording_file_;
    const msr::airlib::PhysicsBody* fpv_physics_body_;

    FString image_path_;

    static FRecordingThread* instance_;
    FRunnableThread* thread_;

    RecordingSettings settings_;
	void saveImage();
    msr::airlib::TTimePoint last_screenshot_on_;
    msr::airlib::Pose last_pose_;
	FGraphEventRef RenderStatus;
	FGraphEventRef CompletionStatus;
    bool is_ready_;
};