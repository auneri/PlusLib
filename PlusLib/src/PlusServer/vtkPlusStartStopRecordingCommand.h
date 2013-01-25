/*=Plus=header=begin======================================================
  Program: Plus
  Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
  See License.txt for details.
=========================================================Plus=header=end*/ 

#ifndef __vtkPlusStartStopRecordingCommand_h
#define __vtkPlusStartStopRecordingCommand_h

#include "vtkPlusCommand.h"

class vtkVirtualStreamDiscCapture;
class vtkDataCollector;

/*!
  \class vtkPlusStartStopRecordingCommand 
  \brief This command starts and stops capturing with a vtkVirtualStreamDiscCapture capture on the server side. 
  \ingroup PlusLibDataCollection
 */ 
class VTK_EXPORT vtkPlusStartStopRecordingCommand : public vtkPlusCommand
{
public:

  static vtkPlusStartStopRecordingCommand *New();
  vtkTypeMacro(vtkPlusStartStopRecordingCommand, vtkPlusCommand);
  virtual void PrintSelf( ostream& os, vtkIndent indent );
  virtual vtkPlusCommand* Clone() { return New(); }

  /*! Executes the command  */
  virtual PlusStatus Execute();

  /*! Read command parameters from XML */
  virtual PlusStatus ReadConfiguration(vtkXMLDataElement* aConfig);

  /*! Write command parameters to XML */
  virtual PlusStatus WriteConfiguration(vtkXMLDataElement* aConfig);

  /*! Get all the command names that this class can execute */
  virtual void GetCommandNames(std::list<std::string> &cmdNames);

  /*! Gets the description for the specified command name. */
  virtual std::string GetDescription(const char* commandName);
  
  vtkGetStringMacro(OutputFilename);
  vtkSetStringMacro(OutputFilename);

  vtkGetStringMacro(CaptureDeviceId);
  vtkSetStringMacro(CaptureDeviceId);

  void SetNameToStart();
  void SetNameToSuspend();
  void SetNameToResume();
  void SetNameToStop();

  /*!
    Helper function to get pointer to the capture device
    \param captureDeviceId Capture device ID. If it is NULL then a pointer to the first VirtualStreamCapture device is returned.
  */
  vtkVirtualStreamDiscCapture* GetCaptureDevice(const char* captureDeviceId);

protected:

  vtkPlusStartStopRecordingCommand();
  virtual ~vtkPlusStartStopRecordingCommand();
  
private:

  char* OutputFilename;
  char* CaptureDeviceId;

  vtkPlusStartStopRecordingCommand( const vtkPlusStartStopRecordingCommand& );
  void operator=( const vtkPlusStartStopRecordingCommand& );
  
};


#endif

