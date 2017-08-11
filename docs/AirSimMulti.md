# Instructions for AirSimMulti


1. Within Unreal Engine, splitscreen would have to be disabled. This can be done by navigating to Unreal->Project Settings->Maps and Modes and by unchecking splitscreen.
2. Depending on the number of drones being simulated, additional PlayerStarts would have to be added to specify the accurate spawn locations for each drone. Each of these PlayerStart icons contains a tag in "PlayerStart->Details", which needs to be named as One, Two, Three and so on. Below is a screenshot for two drones and two corresponding player starts.

![Tagging a player start](docs/images/PlayerStart_tagging.PNG)

3. For HIL to work, within the settings.json file, the right COMports need to be specified based on where the PIXHAWK modules are enumerated. This code has not been tested with SITL yet. A sample settings.json file can be found at [settings.md](docs/settings.md).
4. Each drone that is simulated within Unreal uses its own IP address (127.0.0.1, 0.2, 0.3...) in order to facilitate offboard control from external modules such as DroneShell and PythonClient. 
