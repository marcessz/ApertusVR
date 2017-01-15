#include <iostream>
#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/filewritestream.h"
#include "ApeFobHeadTrackingPlugin.h"

ApeFobHeadTrackingPlugin::ApeFobHeadTrackingPlugin()
{
	mpSystemConfig = Ape::ISystemConfig::getSingletonPtr();
	mpEventManager = Ape::IEventManager::getSingletonPtr();
	mpEventManager->connectEvent(Ape::Event::Group::NODE, std::bind(&ApeFobHeadTrackingPlugin::eventCallBack, this, std::placeholders::_1));
	mpEventManager->connectEvent(Ape::Event::Group::CAMERA, std::bind(&ApeFobHeadTrackingPlugin::eventCallBack, this, std::placeholders::_1));
	mpScene = Ape::IScene::getSingletonPtr();
	mpFobTracker = nullptr;
	mCameraDoubleQueue = Ape::DoubleQueue<Ape::CameraWeakPtr>();
	mDisplayConfigCamerasMap = std::map<Ape::FobHeadTrackingDisplayConfig, std::vector<Ape::CameraWeakPtr>>();
	mpMainWindow = Ape::IMainWindow::getSingletonPtr();
	mTrackerConfig = Ape::FobHeadTrackingTrackerConfig();
	mDisplayConfigList = Ape::FobHeadTrackingDisplayConfigList();
}

ApeFobHeadTrackingPlugin::~ApeFobHeadTrackingPlugin()
{
	std::cout << "ApeFobHeadTrackingPlugin dtor" << std::endl;
}

void ApeFobHeadTrackingPlugin::eventCallBack(const Ape::Event& event)
{
	if (event.type == Ape::Event::Type::NODE_CREATE && event.subjectName == mpSystemConfig->getSceneSessionConfig().generatedUniqueUserName)
	{
		if (auto node = (mpScene->getNode(event.subjectName).lock()))
			mCameraNode = node;
	}
	else if (event.type == Ape::Event::Type::CAMERA_CREATE)
	{
		if (auto camera = std::static_pointer_cast<Ape::ICamera>(mpScene->getEntity(event.subjectName).lock()))
		{
			mCameraDoubleQueue.push(camera);
		}
	}
}

void ApeFobHeadTrackingPlugin::Init()
{
	std::cout << "ApeFobHeadTrackingPlugin::init" << std::endl;
	std::cout << "ApeFobHeadTrackingPlugin waiting for main window" << std::endl;
	while (mpMainWindow->getHandle() == nullptr)
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	std::cout << "ApeFobHeadTrackingPlugin main window was found" << std::endl;
	mpFobTracker = trackdInitTrackerReader(4126);
	
	std::stringstream fileFullPath;
	fileFullPath << mpSystemConfig->getFolderPath() << "\\ApeFobHeadTrackingPlugin.json";
	FILE* apeFobHeadTrackingPluginConfigFile = std::fopen(fileFullPath.str().c_str(), "r");
	char readBuffer[65536];
	if (apeFobHeadTrackingPluginConfigFile)
	{
		rapidjson::FileReadStream jsonFileReaderStream(apeFobHeadTrackingPluginConfigFile, readBuffer, sizeof(readBuffer));
		rapidjson::Document jsonDocument;
		jsonDocument.ParseStream(jsonFileReaderStream);
		if (jsonDocument.IsObject())
		{
			rapidjson::Value& tracker = jsonDocument["tracker"];
			for (rapidjson::Value::MemberIterator trackerMemberIterator = 
					tracker.MemberBegin(); trackerMemberIterator != tracker.MemberEnd(); ++trackerMemberIterator)
			{
				if (trackerMemberIterator->name == "rotation")
				{
					Ape::Degree angle;
					Ape::Vector3 axis;
					for (rapidjson::Value::MemberIterator rotationMemberIterator =
						tracker[trackerMemberIterator->name].MemberBegin();
						rotationMemberIterator != tracker[trackerMemberIterator->name].MemberEnd(); ++rotationMemberIterator)
					{
						if (rotationMemberIterator->name == "angle")
							angle = rotationMemberIterator->value.GetFloat();
						else if (rotationMemberIterator->name == "x")
							axis.x = rotationMemberIterator->value.GetFloat();
						else if (rotationMemberIterator->name == "y")
							axis.y = rotationMemberIterator->value.GetFloat();
						else if (rotationMemberIterator->name == "z")
							axis.z = rotationMemberIterator->value.GetFloat();
					}
					mTrackerConfig.rotation.FromAngleAxis(angle, axis);
				}
				else if (trackerMemberIterator->name == "translate")
				{
					for (rapidjson::Value::MemberIterator translateMemberIterator =
						tracker[trackerMemberIterator->name].MemberBegin();
						translateMemberIterator != tracker[trackerMemberIterator->name].MemberEnd(); ++translateMemberIterator)
					{
						if (rotationMemberIterator->name == "x")
							mTrackerConfig.translate.x = rotationMemberIterator->value.GetFloat();
						else if (rotationMemberIterator->name == "y")
							mTrackerConfig.translate.y = rotationMemberIterator->value.GetFloat();
						else if (rotationMemberIterator->name == "z")
							mTrackerConfig.translate.z = rotationMemberIterator->value.GetFloat();
					}
				}
				else if (trackerMemberIterator->name == "scale")
				{
				    mTrackerConfig.scale = tracker[trackerMemberIterator->name]->value.getFloat();
				}
			}
			for (auto& display : displays.GetArray())
			{
				Ape::FobHeadTrackingDisplayConfig fobHeadTrackingDisplayConfig;
				for (rapidjson::Value::MemberIterator displayMemberIterator = 
					display.MemberBegin(); displayMemberIterator != display.MemberEnd(); ++displayMemberIterator)
				{
					if (displayMemberIterator->name == "size")
					{
						for (rapidjson::Value::MemberIterator sizeMemberIterator = 
							display[displayMemberIterator->name].MemberBegin();
							sizeMemberIterator != display[displayMemberIterator->name].MemberEnd(); ++sizeMemberIterator)
						{
							if (sizeMemberIterator->name == "width")
								fobHeadTrackingDisplayConfig.size.width = resolutionMemberIterator->value.GetFloat();
							else if (resolutionMemberIterator->name == "height")
								fobHeadTrackingDisplayConfig.size.height = resolutionMemberIterator->value.GetFloat();
						}
					}
					else if (displayMemberIterator->name == "position")
					{
						for (rapidjson::Value::MemberIterator positionMemberIterator =
							display[displayMemberIterator->name].MemberBegin();
							positionMemberIterator != display[displayMemberIterator->name].MemberEnd(); ++positionMemberIterator)
						{
							if (positionMemberIterator->name == "x")
								fobHeadTrackingDisplayConfig.position.x = positionMemberIterator->value.GetFloat();
							else if (positionMemberIterator->name == "y")
								fobHeadTrackingDisplayConfig.position.y = positionMemberIterator->value.GetFloat();
							else if (positionMemberIterator->name == "z")
								fobHeadTrackingDisplayConfig.position.z = positionMemberIterator->value.GetFloat();
						}
					}
					else if (displayMemberIterator->name == "orientation")
					{
					    Ape::Degree angle;
						Ape::Vector3 axis;
						for (rapidjson::Value::MemberIterator orientationMemberIterator =
							display[displayMemberIterator->name].MemberBegin();
							orientationMemberIterator != display[displayMemberIterator->name].MemberEnd(); ++orientationMemberIterator)
						{
							if (orientationMemberIterator->name == "angle")
								angle = orientationMemberIterator->value.GetFloat();
							else if (orientationMemberIterator->name == "x")
								axis.x = orientationMemberIterator->value.GetFloat();
							else if (orientationMemberIterator->name == "y")
								axis.y = orientationMemberIterator->value.GetFloat();
							else if (orientationMemberIterator->name == "z")
								axis.z = orientationMemberIterator->value.GetFloat();
						}
						fobHeadTrackingDisplayConfig.orientation.FromAngleAxis(angle, axis);
					}
					else if (displayMemberIterator->name == "disparity")
					{
						fobHeadTrackingDisplayConfig.disparity = display[displayMemberIterator->name]->value.getFloat();
					}
				}
				mDisplayConfigList.push_back(fobHeadTrackingDisplayConfig);
			}
		}
		fclose(apeFobHeadTrackingPluginConfigFile);
	}	
}

void ApeFobHeadTrackingPlugin::Run()
{
    int excpectedCameraCount = mDisplayConfigList.size() * 2;
	int cameraCount = 0;
    while (cameraCount < excpectedCameraCount)
	{
		mCameraDoubleQueue.swap();
		while (!mCameraDoubleQueue.emptyPop())
		{
			mDisplayConfigCamerasMap[mDisplayConfigList[cameraCount % 2]].push_back(mCameraDoubleQueue.front());
			cameraCount++;
			mCameraDoubleQueue.pop();
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	while (mpFobTracker)
	{
		float positionDataFromTracker[3];
		float orientationDataFromTracker[3];
		if (trackdGetPosition(mpFobTracker, 0, positionDataFromTracker) && trackdGetEulerAngles(mpFobTracker, 0, orientationDataFromTracker))
		{
			Ape::Vector3 viewerPosition = Ape::Vector3(positionDataFromTracker[0], positionDataFromTracker[1], positionDataFromTracker[2]) * mTrackerConfig.scale + mTrackerConfig.translate;
			Ape::Quaternion viewerOrientation = Ape::Euler(orientationDataFromTracker[0], orientationDataFromTracker[1], orientationDataFromTracker[2]).toQuaternion() * mTrackerConfig.rotation;
			for(auto const& displayConfig : mDisplayConfigCamerasMap)
			{
				auto cameraLeft = displayConfig.second[0];
				auto cameraRight = displayConfig.second[1];
				Ape::Vector3 viewerLeftEyeRelativeToDisplay =  displayConfig.orientation.Inverse() * (viewerPosition + viewerOrientation * Ape::Vector3(-displayConfig.disparity / 2, 0, 0) - displayConfig.position);
				Ape::Vector3 viewerRightEyeRelativeToDisplay =  displayConfig.orientation.Inverse() * (viewerPosition + viewerOrientation * Ape::Vector3(displayConfig.disparity / 2, 0, 0) - displayConfig.position);
				cameraLeft->setFocalLength(viewerLeftEyeRelativeToDisplay.z);
				cameraRight->setFocalLength(viewerRightEyeRelativeToDisplay.z);
				cameraLeft->setFrustumOffset(-viewerLeftEyeRelativeToDisplay.x, -viewerLeftEyeRelativeToDisplay.y);
				cameraRight->setFrustumOffset(-viewerRightEyeRelativeToDisplay.x, -viewerRightEyeRelativeToDisplay.y);
				cameraLeft->setFOVy(2 * atan((displayConfig.size.height / 2) / cameraLeft->getFocalLength()));
				cameraRight->setFOVy(2 * atan((displayConfig.size.height / 2) / cameraRight->getFocalLength()));
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}
	mpEventManager->disconnectEvent(Ape::Event::Group::NODE, std::bind(&ApeFobHeadTrackingPlugin::eventCallBack, this, std::placeholders::_1));
}

void ApeFobHeadTrackingPlugin::Step()
{

}

void ApeFobHeadTrackingPlugin::Stop()
{

}

void ApeFobHeadTrackingPlugin::Suspend()
{

}

void ApeFobHeadTrackingPlugin::Restart()
{

}
