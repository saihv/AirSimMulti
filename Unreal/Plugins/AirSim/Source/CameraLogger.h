#pragma once
#include <AirSim.h>
#include <thread>
#include <mutex>
#include "ImageUtils.h"
#include "common/Common.hpp"
#include "AirBlueprintLib.h"
#include "SimModeWorldMultirotor.h"
#include "CameraDirector.h"
#include "PIPCamera.h"
#include "MultiRotorConnector.h"
#include "SimModeWorldBase.h"

class FCameraLogger : public FRunnable
{
	static FCameraLogger* Runnable;
	FRunnableThread* Thread;
	ASimModeWorldMultiRotor* GameThread;

	FThreadSafeCounter StopTaskCounter;

	TArray<FColor> imageColor;
	float width = 1280;
	float height = 720;
	bool bReadPixelsStarted = false;
	FRenderCommandFence ReadPixelFence;
	FString imagePath;

public:
	FCameraLogger(FString imagePath, ASimModeWorldMultiRotor* GameThread);
	virtual ~FCameraLogger();
	static FCameraLogger* ThreadInit(FString path, ASimModeWorldMultiRotor* GameThread);
	static void Shutdown();

private:
	virtual bool Init();
	virtual uint32 Run();
	virtual void Stop();

	void SaveImage(int id);
	void ReadPixelsNonBlocking(TArray<FColor>& bmp, unsigned int id);
	void EnsureCompletion();

	unsigned int imagesSaved = 0;
	unsigned int currentId = 1;
	FGraphEventRef RenderStatus;
	FGraphEventRef CompletionStatus;
};