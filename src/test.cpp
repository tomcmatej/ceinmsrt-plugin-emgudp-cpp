#include <EMG_UDP_Simulink.h>
#include <cstdlib>
#include <csignal>
#include <windows.h>
#include <xercesc/util/PlatformUtils.hpp>

bool endMain;

void SigintHandler(int sig)
{
	endMain = true;;
}

int main(void)
{
	endMain = false;
	signal(SIGINT, SigintHandler);

	xercesc::XMLPlatformUtils::Initialize();

	EMGUDPSimulink plugin;
	plugin.setRecord(true);
	plugin.setDirectories("test");
	plugin.init("subjectMTUCalibrated.xml", "executionRT.xml");
	Sleep(5000);
	while (!endMain)
	{

		std::map<std::string, double> data = plugin.GetDataMap();

		std::cout << plugin.getTime() << " : " << data.at("EMG1") << " " << data.at("EMG2") << " " << data.at("EMG3") << " " << data.at("EMG4") << std::endl;
		Sleep(100);
	}

	plugin.stop();
	std::cout << "exit" << std::endl << std::flush;
	return 0;
}