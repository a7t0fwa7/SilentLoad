# SilentLoad
Loads a drivers through NtLoadDriver by setting up the service registry key directly. To be used in engagement for BYOVD, where service creation creates an alert.

## Usage
SilentLoad doesn't drop the driver for you. Refer to the following to lines:  
https://github.com/ioncodes/SilentLoad/blob/3e5b9d5d5180afd09159c2bc02240552eff78fe9/SilentLoad/main.cpp#L6-L7
