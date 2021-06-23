function config(grabber) {

	//InterfacePort 
    	grabber.InterfacePort.set("LineInputToolSelector", "LIN1");
    	grabber.InterfacePort.set("LineInputToolSource", "IIN11");
    	grabber.InterfacePort.set("QuadratureDecoderToolSources", "DIN11_DIN12");
    	grabber.InterfacePort.set("QuadratureDecoderToolForwardDirection", "A_Leads_B");
    	grabber.InterfacePort.set("QuadratureDecoderToolActivation", "AllEdgesAB");
    	grabber.InterfacePort.set("DividerToolSource", "QDC1");
    	grabber.InterfacePort.set("DividerToolEnableControl", "Enable");
    	grabber.InterfacePort.set("DividerToolDivisionFactor", "1");
    	grabber.InterfacePort.set("QuadratureDecoderToolOutputMode", "ForwardOnly");

	//DevicePort
    	grabber.DevicePort.set("CameraControlMethod", "RG");

	//RemotePort 	
    	grabber.RemotePort.set("AcquisitionMode", "Continuous");
    	grabber.RemotePort.set("AcquisitionFrameRate", "500"); 
    	//grabber.RemotePort.set("ExposureMode", "Timed");
    	//grabber.RemotePort.set("ExposureTime", 500);
}

config(grabbers[0]);
