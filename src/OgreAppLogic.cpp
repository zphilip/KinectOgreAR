#include "OgreAppLogic.h"
#include "OgreApp.h"
#include <Ogre.h>
#include "Chrono.h"
#include "StatsFrameListener.h"
#include <OgrePanelOverlayElement.h>
#include "KinectDevice.h"
#include "KinectFrameListener.h"

using namespace Ogre;

OgreAppLogic::OgreAppLogic() : mApplication(0)
{
	// ogre
	mSceneMgr		= 0;
	mViewport		= 0;
	mCamera         = 0;
	mCameraNode     = 0;
	mVideoDevice    = 0;
	mWebcamBufferL8 = 0;
	mObjectNode     = 0;
	mTrackingSystem = 0;
	mStatsFrameListener = 0;
	mAnimState = 0;

	mOISListener.mParent = this;
}

OgreAppLogic::~OgreAppLogic()
{}

// preAppInit
bool OgreAppLogic::preInit(const Ogre::StringVector &commandArgs)
{
	return true;
}

// postAppInit
bool OgreAppLogic::init(void)
{
	createSceneManager();
	createViewport();
	createCamera();
	createScene();
	
	//webcam resolution
	int width  = 320;
	int height = 240;

	mTrackingSystem = new TrackingSystem;

	if (mKinectDeviceManager.size() > 0)
	{
		
		//init the kinect device
		mKinectDevice = mKinectDeviceManager[0];
		if (mKinectDevice->initPrimeSensor() != XN_STATUS_OK)
			return false;
		//create texture 
		mKinectDevice->createOgreColorTexture(colorTextureName,"");
		mKinectDevice->createOgreDepthTexture(depthTextureName,"");
		mKinectDevice->createOgreColoredDepthTexture(coloredDepthTextureName,"");
		//mKinectDevice->setMotorPosition(mKinectMotorPosition);
		createKinectOverlay(colorTextureName, depthTextureName, coloredDepthTextureName);
		
		//init the ArToolkit tracking system
		initTracking(width, height);
		createWebcamPlane(width, height, 45000.0f);	
	
		//mStatsFrameListener = new StatsFrameListener(mApplication->getRenderWindow());
		//mApplication->getOgreRoot()->addFrameListener(mStatsFrameListener);
		//mStatsFrameListener->showDebugOverlay(true);
		createFrameListener();

		mApplication->getKeyboard()->setEventCallback(&mOISListener);
		mApplication->getMouse()->setEventCallback(&mOISListener);
	
		return true;
	}
	else
	{
		Ogre::Exception(Ogre::Exception::ERR_INVALID_STATE, "No Kinect found", "AppLogic");
		return false;
	}
}

void OgreAppLogic::createFrameListener(void)
{
    mFrameListener= new KinectFrameListener(mApplication->getRenderWindow(), mCamera);
    mFrameListener->showDebugOverlay(true);
    mApplication->getOgreRoot()->addFrameListener(mFrameListener);
}

bool OgreAppLogic::preUpdate(Ogre::Real deltaTime)
{
	return true;
}

bool OgreAppLogic::update(Ogre::Real deltaTime)
{
	//If there is a new frame available on video device
	if (mKinectDevice->Update())
	{
		//RGB -> Gray conversion
		//Ogre::PixelUtil::bulkPixelConversion(mVideoDevice->getBufferData(), Ogre::PF_B8G8R8, mWebcamBufferL8, Ogre::PF_L8, mVideoDevice->getWidth()*mVideoDevice->getHeight());

		//Create Gray level PixelBox
		//Ogre::PixelBox box(mVideoDevice->getWidth(), mVideoDevice->getHeight(), 1, Ogre::PF_L8, (void*) mWebcamBufferL8);
		//Ogre::PixelBox box(mVideoDevice->getWidth(), mVideoDevice->getHeight(), 1, Ogre::PF_B8G8R8, (void*) mVideoDevice->getBufferData());
		Ogre::PixelBox box(mKinectDevice->getWidth(), mKinectDevice->getHeight(), 1, Ogre::PF_B8G8R8, (void*) mKinectDevice->getKinectColorBufferData());

		//Tracking using ArToolKitPlus
		mTrackingSystem->update(box);

		if (mTrackingSystem->isPoseComputed())
		{
			mObjectNode->setVisible(true);
			mCameraNode->setOrientation(mTrackingSystem->getOrientation());
			mCameraNode->setPosition(mTrackingSystem->getTranslation());
		}
		else
		{
			mObjectNode->setVisible(true);
		}
	}
	if (mAnimState)
		mAnimState->addTime(deltaTime);

	bool result = processInputs(deltaTime);
	return result;
}

void OgreAppLogic::shutdown(void)
{
	mKinectDevice->shutdown();
	mKinectDevice = NULL;

	delete[] mWebcamBufferL8;
	mWebcamBufferL8 = NULL;

	delete mTrackingSystem;
	mTrackingSystem = NULL;

	mApplication->getOgreRoot()->removeFrameListener(mStatsFrameListener);
	delete mStatsFrameListener;
	mStatsFrameListener = 0;
	
	if(mSceneMgr)
		mApplication->getOgreRoot()->destroySceneManager(mSceneMgr);
	mSceneMgr = 0;
}

void OgreAppLogic::postShutdown(void)
{

}

//--------------------------------- Init --------------------------------

void OgreAppLogic::createSceneManager(void)
{
	mSceneMgr = mApplication->getOgreRoot()->createSceneManager(ST_GENERIC, "SceneManager");
}

void OgreAppLogic::createViewport(void)
{
	mViewport = mApplication->getRenderWindow()->addViewport(0);
}

void OgreAppLogic::createCamera(void)
{
	mCamera = mSceneMgr->createCamera("camera");
	mCamera->setNearClipDistance(0.5);
	mCamera->setFarClipDistance(50000);
	mCamera->setPosition(100, 100, 100);
	mCamera->lookAt(0, 0, 1);
	mCamera->setFOVy(Degree(40)); //FOVy camera Ogre = 40?
	mCamera->setAspectRatio((float) mViewport->getActualWidth() / (float) mViewport->getActualHeight());	
	mViewport->setCamera(mCamera);

	mCameraNode = mSceneMgr->getRootSceneNode()->createChildSceneNode("cameraNode");
	mCameraNode->setPosition(0, 1700, 0);
	mCameraNode->lookAt(Vector3(0, 1700, -1), Node::TS_WORLD);
	mCameraNode->attachObject(mCamera);
	mCameraNode->setFixedYawAxis(true, Vector3::UNIT_Y);
}

void OgreAppLogic::createScene(void)
{
	// setup some basic lighting for our scene
	mSceneMgr->setSkyDome(true, "Examples/CloudySky", 5, 8);
	// make a cube to bounce around
	Entity *ent1;
	SceneNode *boxNode;
	
	ManualObject *cmo = createCubeMesh("manual", "");
	cmo->convertToMesh("cube");
	ent1 = mSceneMgr->createEntity("Cube", "cube.mesh");
	ent1->setCastShadows(true);
	boxNode = mSceneMgr->getRootSceneNode()->createChildSceneNode();
	boxNode->attachObject(ent1);
	boxNode->setScale(Vector3(0.1,0.1,0.1)); // for some reason converttomesh multiplied dimensions by 10
	
    mSceneMgr->setAmbientLight(ColourValue(0.3, 0.3, 0.3));
    mSceneMgr->createLight()->setPosition(20, 80, 50);
	// create a floor mesh resource
	MeshManager::getSingleton().createPlane("floor", ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
	Plane(Vector3::UNIT_Y, -30), 1000, 1000, 10, 10, true, 1, 8, 8, Vector3::UNIT_Z);

	// create a floor entity, give it a material, and place it at the origin
    Entity* floor = mSceneMgr->createEntity("Floor", "floor");
    floor->setMaterialName("Examples/BumpyMetal");
    mSceneMgr->getRootSceneNode()->attachObject(floor);
	
	mSceneMgr->getRootSceneNode()->attachObject(mSceneMgr->createEntity("Head", "ogrehead.mesh"));
	mSceneMgr->setSkyBox(true, "Examples/GridSkyBox");
	
	Ogre::Entity* ent = mSceneMgr->createEntity("Sinbad.mesh");	//1x1_cube.mesh //Sinbad.mesh //axes.mesh

	mObjectNode = mSceneMgr->getRootSceneNode()->createChildSceneNode("cube");	
	mObjectNode->setOrientation(Quaternion(Degree(90.f), Vector3::UNIT_X));
	Ogre::Real scale = 22;
	mObjectNode->setPosition(10, 10, 10*scale);
	mObjectNode->setScale(Ogre::Vector3::UNIT_SCALE*scale);
	mObjectNode->attachObject(ent);
	mObjectNode->setVisible(true);

	// create swords and attach them to sinbad
	Ogre::Entity* sword1 = mSceneMgr->createEntity("SinbadSword1", "Sword.mesh");
	Ogre::Entity* sword2 = mSceneMgr->createEntity("SinbadSword2", "Sword.mesh");
	ent->attachObjectToBone("Sheath.L", sword1);
	ent->attachObjectToBone("Sheath.R", sword2);
	mAnimState = ent->getAnimationState("Dance");
	mAnimState->setLoop(true);
	mAnimState->setEnabled(true);

}

ManualObject* OgreAppLogic::createCubeMesh(Ogre::String name, Ogre::String matName) {

   ManualObject* cube = new ManualObject(name);

   cube->begin(matName);

   cube->position(0.5,-0.5,1.0);cube->normal(0.408248,-0.816497,0.408248);cube->textureCoord(1,0);
   cube->position(-0.5,-0.5,0.0);cube->normal(-0.408248,-0.816497,-0.408248);cube->textureCoord(0,1);
   cube->position(0.5,-0.5,0.0);cube->normal(0.666667,-0.333333,-0.666667);cube->textureCoord(1,1);
   cube->position(-0.5,-0.5,1.0);cube->normal(-0.666667,-0.333333,0.666667);cube->textureCoord(0,0);
   cube->position(0.5,0.5,1.0);cube->normal(0.666667,0.333333,0.666667);cube->textureCoord(1,0);
   cube->position(-0.5,-0.5,1.0);cube->normal(-0.666667,-0.333333,0.666667);cube->textureCoord(0,1);
   cube->position(0.5,-0.5,1.0);cube->normal(0.408248,-0.816497,0.408248);cube->textureCoord(1,1);
   cube->position(-0.5,0.5,1.0);cube->normal(-0.408248,0.816497,0.408248);cube->textureCoord(0,0);
   cube->position(-0.5,0.5,0.0);cube->normal(-0.666667,0.333333,-0.666667);cube->textureCoord(0,1);
   cube->position(-0.5,-0.5,0.0);cube->normal(-0.408248,-0.816497,-0.408248);cube->textureCoord(1,1);
   cube->position(-0.5,-0.5,1.0);cube->normal(-0.666667,-0.333333,0.666667);cube->textureCoord(1,0);
   cube->position(0.5,-0.5,0.0);cube->normal(0.666667,-0.333333,-0.666667);cube->textureCoord(0,1);
   cube->position(0.5,0.5,0.0);cube->normal(0.408248,0.816497,-0.408248);cube->textureCoord(1,1);
   cube->position(0.5,-0.5,1.0);cube->normal(0.408248,-0.816497,0.408248);cube->textureCoord(0,0);
   cube->position(0.5,-0.5,0.0);cube->normal(0.666667,-0.333333,-0.666667);cube->textureCoord(1,0);
   cube->position(-0.5,-0.5,0.0);cube->normal(-0.408248,-0.816497,-0.408248);cube->textureCoord(0,0);
   cube->position(-0.5,0.5,1.0);cube->normal(-0.408248,0.816497,0.408248);cube->textureCoord(1,0);
   cube->position(0.5,0.5,0.0);cube->normal(0.408248,0.816497,-0.408248);cube->textureCoord(0,1);
   cube->position(-0.5,0.5,0.0);cube->normal(-0.666667,0.333333,-0.666667);cube->textureCoord(1,1);
   cube->position(0.5,0.5,1.0);cube->normal(0.666667,0.333333,0.666667);cube->textureCoord(0,0);

   cube->triangle(0,1,2);      cube->triangle(3,1,0);
   cube->triangle(4,5,6);      cube->triangle(4,7,5);
   cube->triangle(8,9,10);      cube->triangle(10,7,8);
   cube->triangle(4,11,12);   cube->triangle(4,13,11);
   cube->triangle(14,8,12);   cube->triangle(14,15,8);
   cube->triangle(16,17,18);   cube->triangle(16,19,17);
   cube->end();

   return cube;
}
void OgreAppLogic::initTracking(int width, int height)
{
	mWebcamBufferL8 = new unsigned char[width*height];
	mTrackingSystem->init(width, height);

	//Create Webcam Material
	MaterialPtr material = MaterialManager::getSingleton().create("WebcamMaterial", ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
	Ogre::Technique *technique = material->createTechnique();
	technique->createPass();
	material->getTechnique(0)->getPass(0)->setLightingEnabled(false);
	material->getTechnique(0)->getPass(0)->setDepthWriteEnabled(false);
	material->getTechnique(0)->getPass(0)->createTextureUnitState(colorTextureName);
}

void OgreAppLogic::createWebcamPlane(int width, int height, Ogre::Real _distanceFromCamera)
{
	// Create a prefab plane dedicated to display video
	float videoAspectRatio = width / (float) height;

	float planeHeight = 2 * _distanceFromCamera * Ogre::Math::Tan(Degree(26)*0.5); //FOVy webcam = 26?(intrinsic param)
	float planeWidth = planeHeight * videoAspectRatio;

	Plane p(Vector3::UNIT_Z, 0.0);
	MeshManager::getSingleton().createPlane("VerticalPlane", ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, p , planeWidth, planeHeight, 1, 1, true, 1, 1, 1, Vector3::UNIT_Y);
	Entity* planeEntity = mSceneMgr->createEntity("VideoPlane", "VerticalPlane"); 
	planeEntity->setMaterialName("WebcamMaterial");
	planeEntity->setRenderQueueGroup(RENDER_QUEUE_WORLD_GEOMETRY_1);

	// Create a node for the plane, inserts it in the scene
	Ogre::SceneNode* node = mCameraNode->createChildSceneNode("planeNode");
	node->attachObject(planeEntity);

	// Update position    
	Vector3 planePos = mCamera->getPosition() + mCamera->getDirection() * _distanceFromCamera;
	node->setPosition(planePos);

	// Update orientation
	node->setOrientation(mCamera->getOrientation());

	mObjectNode->setPosition(mCameraNode->getPosition() + mCamera->getDirection() * 500);
	mObjectNode->setOrientation(mCamera->getOrientation());
}

void OgreAppLogic::createKinectOverlay(const std::string& colorTextureName, const std::string& depthTextureName, const std::string& coloredDepthTextureName)
{
	//Create Color Overlay
	
	{
		//Create Overlay
		Ogre::OverlayManager& overlayManager = Ogre::OverlayManager::getSingleton();
		Ogre::Overlay* overlay = overlayManager.create("KinectColorOverlay");

		//Create Material
		const std::string materialName = "KinectColorMaterial";
		Ogre::MaterialPtr material = MaterialManager::getSingleton().create(materialName, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
		material->getTechnique(0)->getPass(0)->setLightingEnabled(false);
		material->getTechnique(0)->getPass(0)->setDepthWriteEnabled(false);
		material->getTechnique(0)->getPass(0)->createTextureUnitState(colorTextureName);

		//Create Panel
		Ogre::PanelOverlayElement* panel = static_cast<Ogre::PanelOverlayElement*>(overlayManager.createOverlayElement("Panel", "KinectColorPanel"));
		panel->setMetricsMode(Ogre::GMM_PIXELS);
		panel->setMaterialName(materialName);
		panel->setDimensions((float)Kinect::colorWidth/4, (float)Kinect::colorHeight/4);
		panel->setPosition(0.0f, 0.0f);
		overlay->add2D(panel);		
		overlay->setZOrder(300);
		overlay->show(); 
	}
	
	//Create Depth Overlay
	{
		//Create Overlay
		Ogre::OverlayManager& overlayManager = Ogre::OverlayManager::getSingleton();
		Ogre::Overlay* overlay = overlayManager.create("KinectDepthOverlay");

		//Create Material
		const std::string materialName = "KinectDepthMaterial";
		Ogre::MaterialPtr material = MaterialManager::getSingleton().create(materialName, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
		material->getTechnique(0)->getPass(0)->setLightingEnabled(false);
		material->getTechnique(0)->getPass(0)->setDepthWriteEnabled(false);
		material->getTechnique(0)->getPass(0)->setAlphaRejectSettings(CMPF_GREATER, 127);
		material->getTechnique(0)->getPass(0)->createTextureUnitState(depthTextureName);
		//material->getTechnique(0)->getPass(0)->setVertexProgram("Ogre/Compositor/StdQuad_vp");
		//material->getTechnique(0)->getPass(0)->setFragmentProgram("KinectDepth");

		//Create Panel
		Ogre::PanelOverlayElement* panel = static_cast<Ogre::PanelOverlayElement*>(overlayManager.createOverlayElement("Panel", "KinectDepthPanel"));
		panel->setMetricsMode(Ogre::GMM_PIXELS);
		panel->setMaterialName(materialName);
		panel->setDimensions((float)Kinect::depthWidth/4, (float)Kinect::depthHeight/4);
		panel->setPosition((float)640.0f/4, 0.0f);
		overlay->add2D(panel);		
		overlay->setZOrder(310);
		overlay->show();
	}
	
	//Create Colored Depth Overlay
	{
		//Create Overlay
		Ogre::OverlayManager& overlayManager = Ogre::OverlayManager::getSingleton();
		Ogre::Overlay* overlay = overlayManager.create("KinectColoredDepthOverlay");

		//Create Material
		const std::string materialName = "KinectColoredDepthMaterial";
		Ogre::MaterialPtr material = MaterialManager::getSingleton().create(materialName, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
		material->getTechnique(0)->getPass(0)->setLightingEnabled(false);
		material->getTechnique(0)->getPass(0)->setDepthWriteEnabled(false);
		material->getTechnique(0)->getPass(0)->createTextureUnitState(coloredDepthTextureName);

		//Create Panel
		Ogre::PanelOverlayElement* panel = static_cast<Ogre::PanelOverlayElement*>(overlayManager.createOverlayElement("Panel", "KinectColoredDepthPanel"));
		panel->setMetricsMode(Ogre::GMM_PIXELS);
		panel->setMaterialName(materialName);
		panel->setDimensions((float)Kinect::depthWidth/4, (float)Kinect::depthHeight/4);
		panel->setPosition((float)1280.0f/4, 0.0f);
		overlay->add2D(panel);		
		overlay->setZOrder(320);
		overlay->show();
	}
}

//--------------------------------- update --------------------------------

bool OgreAppLogic::processInputs(Ogre::Real deltaTime)
{
	OIS::Keyboard *keyboard = mApplication->getKeyboard();
	if(keyboard->isKeyDown(OIS::KC_ESCAPE))
	{
		return false;
	}

	return true;
}

bool OgreAppLogic::OISListener::mouseMoved( const OIS::MouseEvent &arg )
{
	return true;
}

bool OgreAppLogic::OISListener::mousePressed( const OIS::MouseEvent &arg, OIS::MouseButtonID id )
{
	return true;
}

bool OgreAppLogic::OISListener::mouseReleased( const OIS::MouseEvent &arg, OIS::MouseButtonID id )
{
	return true;
}

bool OgreAppLogic::OISListener::keyPressed( const OIS::KeyEvent &arg )
{
	return true;
}

bool OgreAppLogic::OISListener::keyReleased( const OIS::KeyEvent &arg )
{
	return true;
}