/*=Plus=header=begin======================================================
Program: Plus
Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
See License.txt for details.
=========================================================Plus=header=end*/ 

/*!
  \file vtkFreehandCalibrationTest.cxx 
  \brief This test runs a freehand calibration on a recorded data set and 
  compares the results to a baseline
*/ 

#include "PlusConfigure.h"
#include "vtkPlusConfig.h"
#include "PlusMath.h"
#include "vtkProbeCalibrationAlgo.h"
#include "vtkTransformRepository.h"
#include "FidPatternRecognition.h"

#include "vtkSmartPointer.h"
#include "vtkCommand.h"
#include "vtkCallbackCommand.h"
#include "vtkXMLDataElement.h"
#include "vtkXMLUtilities.h"
#include "vtksys/CommandLineArguments.hxx" 
#include "vtksys/SystemTools.hxx"
#include "vtkMatrix4x4.h"
#include "vtkTransform.h"
#include "vtkTransformRepository.h"
#include "vtkMath.h"
#include "vtkTrackedFrameList.h"

#include <stdlib.h>
#include <iostream>

///////////////////////////////////////////////////////////////////
const double ERROR_THRESHOLD = 0.05; // error threshold is 5% 

int CompareCalibrationResultsWithBaseline(const char* baselineFileName, const char* currentResultFileName, int translationErrorThreshold, int rotationErrorThreshold); 
void PrintLogsCallback(vtkObject* obj, unsigned long eid, void* clientdata, void* calldata); 
double GetCalibrationError(vtkMatrix4x4* baseTransMatrix, vtkMatrix4x4* currentTransMatrix); 

int main (int argc, char* argv[])
{
  std::string inputFreehandMotion1SeqMetafile;
  std::string inputFreehandMotion2SeqMetafile;

  std::string inputConfigFileName;
  std::string inputBaselineFileName;

  double inputTranslationErrorThreshold(0);
  double inputRotationErrorThreshold(0);

  int verboseLevel=vtkPlusLogger::LOG_LEVEL_DEFAULT;

  vtksys::CommandLineArguments cmdargs;
  cmdargs.Initialize(argc, argv);

  cmdargs.AddArgument("--input-freehand-motion-1-sequence-metafile", vtksys::CommandLineArguments::EQUAL_ARGUMENT, &inputFreehandMotion1SeqMetafile, "Sequence metafile name of saved freehand motion 1 dataset.");
  cmdargs.AddArgument("--input-freehand-motion-2-sequence-metafile", vtksys::CommandLineArguments::EQUAL_ARGUMENT, &inputFreehandMotion2SeqMetafile, "Sequence metafile name of saved freehand motion 2 dataset.");

  cmdargs.AddArgument("--input-config-file-name", vtksys::CommandLineArguments::EQUAL_ARGUMENT, &inputConfigFileName, "Configuration file name)");
  cmdargs.AddArgument("--input-baseline-file-name", vtksys::CommandLineArguments::EQUAL_ARGUMENT, &inputBaselineFileName, "Name of file storing baseline calibration results");

  cmdargs.AddArgument("--translation-error-threshold", vtksys::CommandLineArguments::EQUAL_ARGUMENT, &inputTranslationErrorThreshold, "Translation error threshold in mm.");	
  cmdargs.AddArgument("--rotation-error-threshold", vtksys::CommandLineArguments::EQUAL_ARGUMENT, &inputRotationErrorThreshold, "Rotation error threshold in degrees.");	

	cmdargs.AddArgument("--verbose", vtksys::CommandLineArguments::EQUAL_ARGUMENT, &verboseLevel, "Verbose level (1=error only, 2=warning, 3=info, 4=debug, 5=trace)");	

  if ( !cmdargs.Parse() )
  {
    std::cerr << "Problem parsing arguments" << std::endl;
    std::cout << "Help: " << cmdargs.GetHelp() << std::endl;
    exit(EXIT_FAILURE);
  }

  LOG_INFO("Initialize"); 

  // Read configuration
  vtkSmartPointer<vtkXMLDataElement> configRootElement = vtkSmartPointer<vtkXMLDataElement>::Take(vtkXMLUtilities::ReadElementFromFile(inputConfigFileName.c_str()));
  if (configRootElement == NULL)
  {	
    LOG_ERROR("Unable to read configuration from file " << inputConfigFileName.c_str()); 
    return EXIT_FAILURE;
  }
  vtkPlusConfig::GetInstance()->SetDeviceSetConfigurationData(configRootElement);

  vtkPlusLogger::Instance()->SetLogLevel(verboseLevel);

  // Read coordinate definitions
  vtkSmartPointer<vtkTransformRepository> transformRepository = vtkSmartPointer<vtkTransformRepository>::New();
  if ( transformRepository->ReadConfiguration(configRootElement) != PLUS_SUCCESS )
  {
    LOG_ERROR("Failed to read CoordinateDefinitions!"); 
    return EXIT_FAILURE;
  }

  vtkSmartPointer<vtkProbeCalibrationAlgo> freehandCalibration = vtkSmartPointer<vtkProbeCalibrationAlgo>::New();
  freehandCalibration->ReadConfiguration(configRootElement);

  freehandCalibration->Initialize();

  FidPatternRecognition patternRecognition;
  patternRecognition.ReadConfiguration(configRootElement);

  // Load and segment calibration image
  vtkSmartPointer<vtkTrackedFrameList> calibrationTrackedFrameList = vtkSmartPointer<vtkTrackedFrameList>::New();
  if (calibrationTrackedFrameList->ReadFromSequenceMetafile(inputFreehandMotion1SeqMetafile.c_str()) != PLUS_SUCCESS)
  {
    LOG_ERROR("Reading calibration images from '" << inputFreehandMotion1SeqMetafile << "' failed!"); 
    return EXIT_FAILURE;
  }

  int numberOfSuccessfullySegmentedCalibrationImages = 0;
  if (patternRecognition.RecognizePattern(calibrationTrackedFrameList, &numberOfSuccessfullySegmentedCalibrationImages) != PLUS_SUCCESS)
  {
    LOG_ERROR("Error occured during segmentation of calibration images!"); 
    return EXIT_FAILURE;
  }

  LOG_INFO("Segmentation success rate of calibration images: " << numberOfSuccessfullySegmentedCalibrationImages << " out of " << calibrationTrackedFrameList->GetNumberOfTrackedFrames());

  // Load and segment validation image
  vtkSmartPointer<vtkTrackedFrameList> validationTrackedFrameList = vtkSmartPointer<vtkTrackedFrameList>::New();
  if (validationTrackedFrameList->ReadFromSequenceMetafile(inputFreehandMotion2SeqMetafile.c_str()) != PLUS_SUCCESS)
  {
    LOG_ERROR("Reading validation images from '" << inputFreehandMotion2SeqMetafile << "' failed!"); 
    return EXIT_FAILURE;
  }

  int numberOfSuccessfullySegmentedValidationImages = 0;
  if (patternRecognition.RecognizePattern(validationTrackedFrameList, &numberOfSuccessfullySegmentedValidationImages) != PLUS_SUCCESS)
  {
    LOG_ERROR("Error occured during segmentation of validation images!"); 
    return EXIT_FAILURE;
  }

  LOG_INFO("Segmentation success rate of validation images: " << numberOfSuccessfullySegmentedValidationImages << " out of " << validationTrackedFrameList->GetNumberOfTrackedFrames());

  // Calibrate
  if (freehandCalibration->Calibrate( validationTrackedFrameList, calibrationTrackedFrameList, transformRepository, patternRecognition.GetFidLineFinder()->GetNWires()) != PLUS_SUCCESS)
  {
    LOG_ERROR("Calibration failed!");
    return EXIT_FAILURE;
  }

  // Compare results
	std::string currentConfigFileName = vtkPlusConfig::GetInstance()->GetOutputDirectory() + std::string("/") + std::string(vtkPlusConfig::GetInstance()->GetApplicationStartTimestamp()) + ".Calibration.results.xml";
  if ( CompareCalibrationResultsWithBaseline( inputBaselineFileName.c_str(), currentConfigFileName.c_str(), inputTranslationErrorThreshold, inputRotationErrorThreshold ) !=0 )
  {
    LOG_ERROR("Comparison of calibration data to baseline failed");
    std::cout << "Exit failure!!!" << std::endl; 

    return EXIT_FAILURE;
  }

  std::cout << "Exit success!!!" << std::endl; 
  return EXIT_SUCCESS;
}

//-------------------------------------------------------------------------------------------------

// return the number of differences
int CompareCalibrationResultsWithBaseline(const char* baselineFileName, const char* currentResultFileName, int translationErrorThreshold, int rotationErrorThreshold)
{
  int numberOfFailures=0;

  vtkSmartPointer<vtkXMLDataElement> baselineRootElem = vtkSmartPointer<vtkXMLDataElement>::Take(
    vtkXMLUtilities::ReadElementFromFile(baselineFileName));
  vtkSmartPointer<vtkXMLDataElement> currentRootElem = vtkSmartPointer<vtkXMLDataElement>::Take(
    vtkXMLUtilities::ReadElementFromFile(currentResultFileName));
  // check to make sure we have the right element
  if (baselineRootElem == NULL )
  {
    LOG_ERROR("Reading baseline data file failed: " << baselineFileName);
    numberOfFailures++;
    return numberOfFailures;
  }
  if (currentRootElem == NULL )
  {
    LOG_ERROR("Reading newly generated data file failed: " << currentResultFileName);
    numberOfFailures++;
    return numberOfFailures;
  }

  {	//<CalibrationResults>
    vtkXMLDataElement* calibrationResultsBaseline = baselineRootElem->FindNestedElementWithName("CalibrationResults"); 
    vtkXMLDataElement* calibrationResults = currentRootElem->FindNestedElementWithName("CalibrationResults"); 

    if ( calibrationResultsBaseline == NULL) 
    {
      LOG_ERROR("Reading baseline CalibrationResults tag failed: " << baselineFileName);
      numberOfFailures++;
      return numberOfFailures;
    }

    if ( calibrationResults == NULL) 
    {
      LOG_ERROR("Reading current CalibrationResults tag failed: " << currentResultFileName);
      numberOfFailures++;
      return numberOfFailures;
    }

    {	// <CalibrationTransform>
      vtkXMLDataElement* calibrationTransformBaseline = calibrationResultsBaseline->FindNestedElementWithName("CalibrationTransform"); 
      vtkXMLDataElement* calibrationTransform = calibrationResults->FindNestedElementWithName("CalibrationTransform");

      if ( calibrationTransformBaseline == NULL) 
      {
        LOG_ERROR("Reading baseline CalibrationTransform tag failed: " << baselineFileName);
        numberOfFailures++;
        return numberOfFailures;
      }

      if ( calibrationTransform == NULL) 
      {
        LOG_ERROR("Reading current CalibrationTransform tag failed: " << currentResultFileName);
        numberOfFailures++;
        return numberOfFailures;
      }

      //********************************* TransformImageToProbe *************************************
      double blTransformImageToProbe[16]; 
      double cTransformImageToProbe[16]; 

      if (!calibrationTransformBaseline->GetVectorAttribute("TransformImageToProbe", 16, blTransformImageToProbe))
      {
        LOG_ERROR("Baseline TransformImageToProbe tag is missing");
        numberOfFailures++;			
      }
      else if (!calibrationTransform->GetVectorAttribute("TransformImageToProbe", 16, cTransformImageToProbe))
      {
        LOG_ERROR("Current TransformImageToProbe tag is missing");
        numberOfFailures++;			
      }
      else
      { 
        vtkSmartPointer<vtkMatrix4x4> baseTransMatrix = vtkSmartPointer<vtkMatrix4x4>::New(); 
        vtkSmartPointer<vtkMatrix4x4> currentTransMatrix = vtkSmartPointer<vtkMatrix4x4>::New(); 
        for ( int i = 0; i < 4; i++) 
        {
          for ( int j = 0; j < 4; j++)
          {

            baseTransMatrix->SetElement(i,j, blTransformImageToProbe[4*i + j]); 
            currentTransMatrix->SetElement(i,j, cTransformImageToProbe[4*i + j]); 
          }

        }

        double translationError = PlusMath::GetPositionDifference(baseTransMatrix, currentTransMatrix); 
        if ( translationError > translationErrorThreshold )
        {
          LOG_ERROR("TransformImageToProbe translation error is higher than expected: " << translationError << " mm (threshold: " << translationErrorThreshold << " mm). " );
          numberOfFailures++;
        }

        double rotationError = PlusMath::GetOrientationDifference(baseTransMatrix, currentTransMatrix); 
        if ( rotationError > rotationErrorThreshold )
        {
          LOG_ERROR("TransformImageToProbe rotation error is higher than expected: " << rotationError << " degree (threshold: " << rotationErrorThreshold << " degree). " );
          numberOfFailures++;
        }
      }

    }//</CalibrationTransform>

  }//</CalibrationResults>

  {	// <ErrorReports>
    vtkXMLDataElement* errorReportsBaseline = baselineRootElem->FindNestedElementWithName("ErrorReports"); 
    vtkXMLDataElement* errorReports = currentRootElem->FindNestedElementWithName("ErrorReports");

    if ( errorReportsBaseline == NULL) 
    {
      LOG_ERROR("Reading baseline ErrorReports tag failed: " << baselineFileName);
      numberOfFailures++;
      return numberOfFailures;
    }

    if ( errorReports == NULL) 
    {
      LOG_ERROR("Reading current ErrorReports tag failed: " << currentResultFileName);
      numberOfFailures++;
      return numberOfFailures;
    }

    {	// <PointReconstructionErrorAnalysis>
      vtkXMLDataElement* pointReconstructionErrorAnalysisBaseline = errorReportsBaseline->FindNestedElementWithName("PointReconstructionErrorAnalysis"); 
      vtkXMLDataElement* pointReconstructionErrorAnalysis = errorReports->FindNestedElementWithName("PointReconstructionErrorAnalysis");

      if ( pointReconstructionErrorAnalysisBaseline == NULL) 
      {
        LOG_ERROR("Reading baseline PointReconstructionErrorAnalysis tag failed: " << baselineFileName);
        numberOfFailures++;
        return numberOfFailures;
      }

      if ( pointReconstructionErrorAnalysis == NULL) 
      {
        LOG_ERROR("Reading current PointReconstructionErrorAnalysis tag failed: " << currentResultFileName);
        numberOfFailures++;
        return numberOfFailures;
      }

      double blPRE[9]; 
      double cPRE[9]; 

      if (!pointReconstructionErrorAnalysisBaseline->GetVectorAttribute("PRE", 9, blPRE))
      {
        LOG_ERROR("Baseline PRE is missing");
        numberOfFailures++;			
      }
      else if (!pointReconstructionErrorAnalysis->GetVectorAttribute("PRE", 9, cPRE))
      {
        LOG_ERROR("Current PRE is missing");
        numberOfFailures++;			
      }
      else
      {
        for ( int i = 0; i < 9; i++) 
        {
          double ratio = 1.0 * blPRE[i] / cPRE[i]; 

          if ( ratio > 1 + ERROR_THRESHOLD || ratio < 1 - ERROR_THRESHOLD )
          {
            LOG_ERROR("PRE element (" << i << ") mismatch: current=" << cPRE[i]<< ", baseline=" << blPRE[i]);
            numberOfFailures++;
          }
        }
      }

      double blValidationDataConfidenceLevel, cValidationDataConfidenceLevel; 
      if (!pointReconstructionErrorAnalysisBaseline->GetScalarAttribute("ValidationDataConfidenceLevel", blValidationDataConfidenceLevel))
      {
        LOG_ERROR("Baseline PRE ValidationDataConfidenceLevel is missing");
        numberOfFailures++;			
      }
      else if (!pointReconstructionErrorAnalysis->GetScalarAttribute("ValidationDataConfidenceLevel", cValidationDataConfidenceLevel))
      {
        LOG_ERROR("Current PRE ValidationDataConfidenceLevel is missing");
        numberOfFailures++;			
      }
      else
      {
        double ratio = 1.0 * blValidationDataConfidenceLevel / cValidationDataConfidenceLevel; 

        if ( ratio > 1 + ERROR_THRESHOLD || ratio < 1 - ERROR_THRESHOLD )
        {
          LOG_ERROR("PRE ValidationDataConfidenceLevel mismatch: current=" << cValidationDataConfidenceLevel << ", baseline=" << blValidationDataConfidenceLevel);
          numberOfFailures++;
        }
      }

    }// </PointReconstructionErrorAnalysis>

    {	// <PointLineDistanceErrorAnalysis>
      vtkXMLDataElement* pointLineDistanceErrorAnalysisBaseline = errorReportsBaseline->FindNestedElementWithName("PointLineDistanceErrorAnalysis"); 
      vtkXMLDataElement* pointLineDistanceErrorAnalysis = errorReports->FindNestedElementWithName("PointLineDistanceErrorAnalysis");

      if ( pointLineDistanceErrorAnalysisBaseline == NULL) 
      {
        LOG_ERROR("Reading baseline PointLineDistanceErrorAnalysis tag failed: " << baselineFileName);
        numberOfFailures++;
        return numberOfFailures;
      }

      if ( pointLineDistanceErrorAnalysis == NULL) 
      {
        LOG_ERROR("Reading current PointLineDistanceErrorAnalysis tag failed: " << currentResultFileName);
        numberOfFailures++;
        return numberOfFailures;
      }

      double blPLDE[3]; 
      double cPLDE[3]; 

      if (!pointLineDistanceErrorAnalysisBaseline->GetVectorAttribute("PLDE", 3, blPLDE))
      {
        LOG_ERROR("Baseline PLDE is missing");
        numberOfFailures++;			
      }
      else if (!pointLineDistanceErrorAnalysis->GetVectorAttribute("PLDE", 3, cPLDE))
      {
        LOG_ERROR("Current PLDE is missing");
        numberOfFailures++;			
      }
      else
      {
        for ( int i = 0; i < 3; i++) 
        {
          double ratio = 1.0 * blPLDE[i] / cPLDE[i]; 

          if ( ratio > 1 + ERROR_THRESHOLD || ratio < 1 - ERROR_THRESHOLD )
          {
            LOG_ERROR("PLDE element (" << i << ") mismatch: current=" << cPLDE[i]<< ", baseline=" << blPLDE[i]);
            numberOfFailures++;
          }
        }
      }

      double blValidationDataConfidenceLevel, cValidationDataConfidenceLevel; 
      if (!pointLineDistanceErrorAnalysisBaseline->GetScalarAttribute("ValidationDataConfidenceLevel", blValidationDataConfidenceLevel))
      {
        LOG_ERROR("Baseline PLDE ValidationDataConfidenceLevel is missing");
        numberOfFailures++;			
      }
      else if (!pointLineDistanceErrorAnalysis->GetScalarAttribute("ValidationDataConfidenceLevel", cValidationDataConfidenceLevel))
      {
        LOG_ERROR("Current PLDE ValidationDataConfidenceLevel is missing");
        numberOfFailures++;			
      }
      else
      {
        double ratio = 1.0 * blValidationDataConfidenceLevel / cValidationDataConfidenceLevel; 

        if ( ratio > 1 + ERROR_THRESHOLD || ratio < 1 - ERROR_THRESHOLD )
        {
          LOG_ERROR("PLDE ValidationDataConfidenceLevel mismatch: current=" << cValidationDataConfidenceLevel << ", baseline=" << blValidationDataConfidenceLevel);
          numberOfFailures++;
        }
      }

    }// </PointLineDistanceErrorAnalysis>
  } //</ErrorReports>

  return numberOfFailures; 

}

double GetCalibrationError(vtkMatrix4x4* baseTransMatrix, vtkMatrix4x4* currentTransMatrix)
{
  vtkSmartPointer<vtkTransform> baseTransform = vtkSmartPointer<vtkTransform>::New(); 
  baseTransform->SetMatrix(baseTransMatrix); 

  vtkSmartPointer<vtkTransform> currentTransform = vtkSmartPointer<vtkTransform>::New(); 
  currentTransform->SetMatrix(currentTransMatrix); 

  double bx = baseTransform->GetPosition()[0]; 
  double by = baseTransform->GetPosition()[1]; 
  double bz = baseTransform->GetPosition()[2]; 

  double cx = currentTransform->GetPosition()[0]; 
  double cy = currentTransform->GetPosition()[1]; 
  double cz = currentTransform->GetPosition()[2]; 

  // Euclidean distance
  double distance = sqrt( pow(bx-cx,2) + pow(by-cy,2) + pow(bz-cz,2) ); 

  return distance; 
}

// Callback function for error and warning redirects
void PrintLogsCallback(vtkObject* obj, unsigned long eid, void* clientdata, void* calldata)
{
  if ( eid == vtkCommand::GetEventIdFromString("WarningEvent") )
  {
    LOG_WARNING((const char*)calldata);
  }
  else if ( eid == vtkCommand::GetEventIdFromString("ErrorEvent") )
  {
    LOG_ERROR((const char*)calldata);
  }
}


