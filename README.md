<img src="https://github.com/CEINMS-RT/ceinmsrt-core-cpp/blob/main/CEINMS-RT_V2_ICON.png" width="50%" alt="CEINMS-RT logo">

[![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg)]()

CEINMS-RT [installation](https://ceinms-docs.readthedocs.io/en/latest/Installation%20%5BWindows%5D.html) and [use](https://ceinms-docs.readthedocs.io/en/latest/Tutorial%20%5BWindows%5D%5BUbuntu%5D.html).
Plugin [installation](#Installation) and [compilation](https://ceinms-docs.readthedocs.io/en/latest/Compilation%20%5BWindows%5D.html). How to use a [plugin](#How-to-use-it).

# EMG UDP Plugin

Plugin that reads from UDP port (does not matter whether MATLAB, Python, or Simulink) and converts data to existing EMG data file. 

The script assumes data to be sent in a certain string manner with preset delimiters. Changes to how the buffer writes the delimiters would require editing the code. 
