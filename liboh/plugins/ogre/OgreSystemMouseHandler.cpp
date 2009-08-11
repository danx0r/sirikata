/*  Sirikata liboh -- Ogre Graphics Plugin
 *  OgreSystemMouseHandler.cpp
 *
 *  Copyright (c) 2009, Patrick Reiter Horn
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of Sirikata nor the names of its contributors may
 *    be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <util/Standard.hh>
#include <oh/Platform.hpp>
#include "OgreSystem.hpp"
#include "CameraEntity.hpp"
#include "LightEntity.hpp"
#include "MeshEntity.hpp"
#include "input/SDLInputManager.hpp"
#include <oh/ProxyManager.hpp>
#include <oh/ProxyPositionObject.hpp>
#include <oh/ProxyMeshObject.hpp>
#include <oh/ProxyLightObject.hpp>
#include "input/InputEvents.hpp"
#include "input/SDLInputDevice.hpp"
#include "DragActions.hpp"
#include <task/Event.hpp>
#include <task/Time.hpp>
#include <task/EventManager.hpp>
#include <SDL_keysym.h>
#include <set>
#include <oh/BulletSystem.hpp>

namespace Sirikata {
namespace Graphics {
using namespace Input;
using namespace Task;

#define DEG2RAD 0.0174532925
#ifdef _WIN32
#undef SDL_SCANCODE_UP
#define SDL_SCANCODE_UP 0x60
#undef SDL_SCANCODE_RIGHT
#define SDL_SCANCODE_RIGHT 0x5e
#undef SDL_SCANCODE_DOWN
#define SDL_SCANCODE_DOWN 0x5a
#undef SDL_SCANCODE_LEFT
#define SDL_SCANCODE_LEFT 0x5c
#undef SDL_SCANCODE_PAGEUP
#define SDL_SCANCODE_PAGEUP 0x61
#undef SDL_SCANCODE_PAGEDOWN
#define SDL_SCANCODE_PAGEDOWN 0x5b
#endif

bool compareEntity (const Entity* one, const Entity* two) {
    Task::AbsTime now = Task::AbsTime::now();
    ProxyPositionObject *pp = one->getProxyPtr().get();
    Location loc1 = pp->globalLocation(now);
    ProxyCameraObject* camera1 = dynamic_cast<ProxyCameraObject*>(pp);
    ProxyLightObject* light1 = dynamic_cast<ProxyLightObject*>(pp);
    ProxyMeshObject* mesh1 = dynamic_cast<ProxyMeshObject*>(pp);
    pp = two->getProxyPtr().get();
    Location loc2 = pp->globalLocation(now);
    ProxyCameraObject* camera2 = dynamic_cast<ProxyCameraObject*>(pp);
    ProxyLightObject* light2 = dynamic_cast<ProxyLightObject*>(pp);
    ProxyMeshObject* mesh2 = dynamic_cast<ProxyMeshObject*>(pp);
    if (camera1 && !camera2) return true;
    if (camera2 && !camera1) return false;
    if (camera1 && camera2) {
        return loc1.getPosition().x < loc2.getPosition().x;
    }
    if (light1 && mesh2) return true;
    if (mesh1 && light2) return false;
    if (light1 && light2) {
        return loc1.getPosition().x < loc2.getPosition().x;
    }
    if (mesh1 && mesh2) {
        return mesh1->getPhysical().name < mesh2->getPhysical().name;
    }
    return one<two;
}

// Defined in DragActions.cpp.

class OgreSystem::MouseHandler {
    OgreSystem *mParent;
    std::vector<SubscriptionId> mEvents;
    typedef std::multimap<InputDevice*, SubscriptionId> DeviceSubMap;
    DeviceSubMap mDeviceSubscriptions;

    SpaceObjectReference mCurrentGroup;
    typedef std::set<ProxyPositionObjectPtr> SelectedObjectSet;
    SelectedObjectSet mSelectedObjects;
    SpaceObjectReference mLastShiftSelected;
    int mLastHitCount;
    float mLastHitX;
    float mLastHitY;
    // map from mouse button to drag for that mouse button.
    /* as far as multiple cursors are concerned,
       each cursor should have its own MouseHandler instance */
    std::map<int, DragAction> mDragAction;
    std::map<int, ActiveDrag*> mActiveDrag;
    /*
        typedef EventResponse (MouseHandler::*ClickAction) (EventPtr evbase);
        std::map<int, ClickAction> mClickAction;
    */

    class SubObjectIterator {
        typedef Entity* value_type;
        //typedef ssize_t difference_type;
        typedef size_t size_type;
        OgreSystem::SceneEntitiesMap::const_iterator mIter;
        Entity *mParentEntity;
        OgreSystem *mOgreSys;
        void findNext() {
            while (!end() && !((*mIter).second->getProxy().getParent() == mParentEntity->id())) {
                ++mIter;
            }
        }
    public:
        SubObjectIterator(Entity *parent) :
                mParentEntity(parent),
                mOgreSys(parent->getScene()) {
            mIter = mOgreSys->mSceneEntities.begin();
            findNext();
        }
        SubObjectIterator &operator++() {
            ++mIter;
            findNext();
            return *this;
        }
        Entity *operator*() const {
            return (*mIter).second;
        }
        bool end() const {
            return (mIter == mOgreSys->mSceneEntities.end());
        }
    };


    /////////////////// HELPER FUNCTIONS ///////////////

    Entity *hoverEntity (CameraEntity *cam, Task::AbsTime time, float xPixel, float yPixel, int *hitCount,int which=0) {
        Location location(cam->getProxy().globalLocation(time));
        Vector3f dir (pixelToDirection(cam, location.getOrientation(), xPixel, yPixel));
        SILOG(input,info,"X is "<<xPixel<<"; Y is "<<yPixel<<"; pos = "<<location.getPosition()<<"; dir = "<<dir << "; which=" << which);

        double dist;
        Vector3f normal;
        Entity *mouseOverEntity = mParent->rayTrace(location.getPosition(), dir, *hitCount, dist, normal, which);
        if (mouseOverEntity) {
            while (!(mouseOverEntity->getProxy().getParent() == mCurrentGroup)) {
                mouseOverEntity = mParent->getEntity(mouseOverEntity->getProxy().getParent());
                if (mouseOverEntity == NULL) {
                    return NULL; // FIXME: should try again.
                }
            }
            return mouseOverEntity;
        }
        return NULL;
    }

    ///////////////////// CLICK HANDLERS /////////////////////
    Task::AbsTime mStartTime;
public:
    void clearSelection() {
        hiliteSelection(true);      /// stop flashing
        for (SelectedObjectSet::const_iterator selectIter = mSelectedObjects.begin();
                selectIter != mSelectedObjects.end(); ++selectIter) {
            Entity *ent = mParent->getEntity((*selectIter)->getObjectReference());
            if (ent) {
                ent->setSelected(false);
            }
            // Fire deselected event.
        }
        mSelectedObjects.clear();
    }

    void hiliteSelection(bool force=false) {
        /// for now, flash selection
        Task::DeltaTime dt=Task::AbsTime::now()-mStartTime;
        bool show;
        if (!force)
            show=int(dt.toSeconds()*1000) % 1000 > 250;
        else
            show=true;
        for (SelectedObjectSet::const_iterator selectIter = mSelectedObjects.begin();
                selectIter != mSelectedObjects.end(); ++selectIter) {
            Entity *ent = mParent->getEntity((*selectIter)->getObjectReference());
            if (ent) {
                //ent->setSelected(mCounter>0x80?true:false);
                ent->setVisible(show);
            }
        }
    }
private:
    bool recentMouseInRange(float x, float y, float *lastX, float *lastY) {
        float delx = x-*lastX;
        float dely = y-*lastY;

        if (delx<0) delx=-delx;
        if (dely<0) dely=-dely;
        if (delx>.03125||dely>.03125) {
            *lastX=x;
            *lastY=y;

            return false;
        }
        return true;
    }
    int mWhichRayObject;
    EventResponse selectObject(EventPtr ev, int direction) {
        std::tr1::shared_ptr<MouseClickEvent> mouseev (
            std::tr1::dynamic_pointer_cast<MouseClickEvent>(ev));
        if (!mouseev) {
            return EventResponse::nop();
        }
        CameraEntity *camera = mParent->mPrimaryCamera;
        if (mParent->mInputManager->isModifierDown(InputDevice::MOD_SHIFT)) {
            // add object.
            int numObjectsUnderCursor=0;
            Entity *mouseOver = hoverEntity(camera, Task::AbsTime::now(), mouseev->mX, mouseev->mY, &numObjectsUnderCursor, mWhichRayObject);
            if (!mouseOver) {
                return EventResponse::nop();
            }
            if (mouseOver->id() == mLastShiftSelected && numObjectsUnderCursor==mLastHitCount ) {
                SelectedObjectSet::iterator selectIter = mSelectedObjects.find(mouseOver->getProxyPtr());
                if (selectIter != mSelectedObjects.end()) {
                    Entity *ent = mParent->getEntity((*selectIter)->getObjectReference());
                    if (ent) {
                        ent->setSelected(false);
                    }
                    mSelectedObjects.erase(selectIter);
                }
                mWhichRayObject+=direction;              /// comment out to always force top object
                mLastShiftSelected = SpaceObjectReference::null();
            }
            else {
                mWhichRayObject=0;
            }
            mouseOver = hoverEntity(camera, Task::AbsTime::now(), mouseev->mX, mouseev->mY, &mLastHitCount, mWhichRayObject);
            if (!mouseOver) {
                return EventResponse::nop();
            }

            SelectedObjectSet::iterator selectIter = mSelectedObjects.find(mouseOver->getProxyPtr());
            if (selectIter == mSelectedObjects.end()) {
                SILOG(input,info,"Added selection " << mouseOver->id());
                mSelectedObjects.insert(mouseOver->getProxyPtr());
                mouseOver->setSelected(true);
                mLastShiftSelected = mouseOver->id();
                // Fire selected event.
            }
            else {
                SILOG(input,info,"Deselected " << (*selectIter)->getObjectReference());
                Entity *ent = mParent->getEntity((*selectIter)->getObjectReference());
                if (ent) {
                    ent->setSelected(false);
                }
                mSelectedObjects.erase(selectIter);
                // Fire deselected event.
            }
        }
        else if (mParent->mInputManager->isModifierDown(InputDevice::MOD_CTRL)) {
            SILOG(input,info,"Cleared selection");
            clearSelection();
            mLastShiftSelected = SpaceObjectReference::null();
        }
        else {
            // reset selection.
            clearSelection();
            mWhichRayObject+=direction;       /// comment out to force selection of nearest object
            int numObjectsUnderCursor=0;
            Entity *mouseOver = hoverEntity(camera, Task::AbsTime::now(), mouseev->mX, mouseev->mY, &numObjectsUnderCursor, mWhichRayObject);
            if (recentMouseInRange(mouseev->mX, mouseev->mY, &mLastHitX, &mLastHitY)==false||numObjectsUnderCursor!=mLastHitCount) {
                mouseOver = hoverEntity(camera, Task::AbsTime::now(), mouseev->mX, mouseev->mY, &mLastHitCount, mWhichRayObject=0);
            }
            if (mouseOver) {
                mSelectedObjects.insert(mouseOver->getProxyPtr());
                mouseOver->setSelected(true);
                SILOG(input,info,"Replaced selection with " << mouseOver->id());
                // Fire selected event.
            }
            mLastShiftSelected = SpaceObjectReference::null();
        }
        return EventResponse::cancel();
    }

///////////////////// DRAG HANDLERS //////////////////////

    EventResponse wheelListener(EventPtr evbase) {
        std::tr1::shared_ptr<AxisEvent> ev(std::tr1::dynamic_pointer_cast<AxisEvent>(evbase));
        if (!ev) {
            return EventResponse::nop();
        }
        if (ev->mAxis == SDLMouse::WHEELY || ev->mAxis == SDLMouse::RELY) {
            AxisValue av = ev->mValue;
            av.value *= 0.2;
            zoomInOut(av, ev->getDevice(), mParent->mPrimaryCamera, mSelectedObjects, mParent);
        }
        else if (ev->mAxis == SDLMouse::WHEELX || ev->mAxis == PointerDevice::RELX) {
            //orbitObject(Vector3d(ev->mValue.getCentered() * AXIS_TO_RADIANS, 0, 0), ev->getDevice());
        }
        return EventResponse::cancel();
    }

    ///////////////// KEYBOARD HANDLERS /////////////////

    EventResponse deleteObjects(EventPtr ev) {
        Task::AbsTime now(Task::AbsTime::now());
        while (doUngroupObjects(now)) {
        }
        for (SelectedObjectSet::iterator iter = mSelectedObjects.begin();
                iter != mSelectedObjects.end(); ++iter) {
            Entity *ent = mParent->getEntity((*iter)->getObjectReference());
            if (ent) {
                ent->getProxy().getProxyManager()->destroyObject(ent->getProxyPtr());
            }
        }
        mSelectedObjects.clear();
        return EventResponse::nop();
    }

    Entity *doCloneObject(Entity *ent, const ProxyPositionObjectPtr &parentPtr, Task::AbsTime now) {
        SpaceObjectReference newId = SpaceObjectReference(ent->id().space(), ObjectReference(UUID::random()));
        Location loc = ent->getProxy().globalLocation(now);
        Location localLoc = ent->getProxy().extrapolateLocation(now);
        ProxyManager *proxyMgr = ent->getProxy().getProxyManager();

        std::tr1::shared_ptr<ProxyMeshObject> meshObj(
            std::tr1::dynamic_pointer_cast<ProxyMeshObject>(ent->getProxyPtr()));
        std::tr1::shared_ptr<ProxyLightObject> lightObj(
            std::tr1::dynamic_pointer_cast<ProxyLightObject>(ent->getProxyPtr()));
        ProxyPositionObjectPtr newObj;
        if (meshObj) {
            std::tr1::shared_ptr<ProxyMeshObject> newMeshObject (new ProxyMeshObject(proxyMgr, newId));
            newObj = newMeshObject;
            proxyMgr->createObject(newMeshObject);
            newMeshObject->setMesh(meshObj->getMesh());
            newMeshObject->setScale(meshObj->getScale());
        }
        else if (lightObj) {
            std::tr1::shared_ptr<ProxyLightObject> newLightObject (new ProxyLightObject(proxyMgr, newId));
            newObj = newLightObject;
            proxyMgr->createObject(newLightObject);
            newLightObject->update(lightObj->getLastLightInfo());
        }
        else {
            newObj = ProxyPositionObjectPtr(new ProxyMeshObject(proxyMgr, newId));
            proxyMgr->createObject(newObj);
        }
        if (newObj) {
            if (parentPtr) {
                newObj->setParent(parentPtr, now, loc, localLoc);
                newObj->resetPositionVelocity(now, localLoc);
            }
            else {
                newObj->resetPositionVelocity(now, loc);
            }
        }
        {
            std::list<Entity*> toClone;
            for (SubObjectIterator subIter (ent); !subIter.end(); ++subIter) {
                toClone.push_back(*subIter);
            }
            for (std::list<Entity*>::const_iterator iter = toClone.begin(); iter != toClone.end(); ++iter) {
                doCloneObject(*iter, newObj, now);
            }
        }
        return mParent->getEntity(newId);
    }
    EventResponse cloneObjects(EventPtr ev) {
        float WORLD_SCALE = mParent->mInputManager->mWorldScale->as<float>();
        Task::AbsTime now(Task::AbsTime::now());
        SelectedObjectSet newSelectedObjects;
        for (SelectedObjectSet::iterator iter = mSelectedObjects.begin();
                iter != mSelectedObjects.end(); ++iter) {
            Entity *ent = mParent->getEntity((*iter)->getObjectReference());
            if (!ent) {
                continue;
            }
            Entity *newEnt = doCloneObject(ent, ent->getProxy().getParentProxy(), now);
            Location loc (ent->getProxy().extrapolateLocation(now));
            loc.setPosition(loc.getPosition() + Vector3d(WORLD_SCALE/2.,0,0));
            newEnt->getProxy().resetPositionVelocity(now, loc);
            newSelectedObjects.insert(newEnt->getProxyPtr());
            newEnt->setSelected(true);
            ent->setSelected(false);
        }
        mSelectedObjects.swap(newSelectedObjects);
        return EventResponse::nop();
    }
    EventResponse groupObjects(EventPtr ev) {
        if (mSelectedObjects.size()<2) {
            return EventResponse::nop();
        }
        SpaceObjectReference parentId = mCurrentGroup;
        Task::AbsTime now(Task::AbsTime::now());
        ProxyManager *proxyMgr = mParent->mPrimaryCamera->getProxy().getProxyManager();
        std::string groupname;
        for (SelectedObjectSet::iterator iter = mSelectedObjects.begin();
                iter != mSelectedObjects.end(); ++iter) {
            Entity *ent = mParent->getEntity((*iter)->getObjectReference());
            if (!ent) continue;
            if (ent->getProxy().getProxyManager() != proxyMgr) {
                SILOG(input,error,"Attempting to group objects owned by different proxy manager!");
                return EventResponse::nop();
            }
            if (!(ent->getProxy().getParent() == parentId)) {
                SILOG(input,error,"Multiple select "<< (*iter)->getObjectReference() <<
                      " has parent  "<<ent->getProxy().getParent() << " instead of " << mCurrentGroup);
                return EventResponse::nop();
            }
            ProxyMeshObject*meshobj = dynamic_cast<ProxyMeshObject*>(ent->getProxyPtr().get());
            if (meshobj && !meshobj->getPhysical().name.empty()) {
                groupname = meshobj->getPhysical().name;
            }
        }
        Vector3d totalPosition (averageSelectedPosition(now, mSelectedObjects.begin(), mSelectedObjects.end()));
        Location totalLocation (totalPosition,Quaternion::identity(),Vector3f(0,0,0),Vector3f(0,0,0),0);
        Entity *parentEntity = mParent->getEntity(parentId);
        if (parentEntity) {
            totalLocation = parentEntity->getProxy().globalLocation(now);
            totalLocation.setPosition(totalPosition);
        }

        SpaceObjectReference newParentId = SpaceObjectReference(mCurrentGroup.space(), ObjectReference(UUID::random()));
        ProxyMeshObject* groupobj = new ProxyMeshObject(proxyMgr, newParentId);
        physicalParameters props = groupobj->getPhysical();
        if (!groupname.empty()) {
            props.name = "grp-" + groupname;
        } else {
            props.name = "group";
        }
        groupobj->setPhysical(props);
        proxyMgr->createObject(ProxyObjectPtr(groupobj));
        Entity *newParentEntity = mParent->getEntity(newParentId);
        newParentEntity->getProxy().resetPositionVelocity(now, totalLocation);

        if (parentEntity) {
            newParentEntity->getProxy().setParent(parentEntity->getProxyPtr(), now);
        }
        for (SelectedObjectSet::iterator iter = mSelectedObjects.begin();
                iter != mSelectedObjects.end(); ++iter) {
            Entity *ent = mParent->getEntity((*iter)->getObjectReference());
            if (!ent) continue;
            ent->getProxy().setParent(newParentEntity->getProxyPtr(), now);
            ent->setSelected(false);
        }
        mSelectedObjects.clear();
        mSelectedObjects.insert(newParentEntity->getProxyPtr());
        newParentEntity->setSelected(true);
        return EventResponse::nop();
    }

    bool doUngroupObjects(Task::AbsTime now) {
        int numUngrouped = 0;
        SelectedObjectSet newSelectedObjects;
        for (SelectedObjectSet::iterator iter = mSelectedObjects.begin();
                iter != mSelectedObjects.end(); ++iter) {
            Entity *parentEnt = mParent->getEntity((*iter)->getObjectReference());
            if (!parentEnt) {
                continue;
            }
            ProxyManager *proxyMgr = parentEnt->getProxy().getProxyManager();
            ProxyPositionObjectPtr parentParent (parentEnt->getProxy().getParentProxy());
            mCurrentGroup = parentEnt->getProxy().getParent(); // parentParent may be NULL.
            bool hasSubObjects = false;
            for (SubObjectIterator subIter (parentEnt); !subIter.end(); ++subIter) {
                hasSubObjects = true;
                Entity *ent = *subIter;
                ent->getProxy().setParent(parentParent, now);
                newSelectedObjects.insert(ent->getProxyPtr());
                ent->setSelected(true);
            }
            if (hasSubObjects) {
                parentEnt->setSelected(false);
                proxyMgr->destroyObject(parentEnt->getProxyPtr());
                parentEnt = NULL; // dies.
                numUngrouped++;
            }
            else {
                newSelectedObjects.insert(parentEnt->getProxyPtr());
            }
        }
        mSelectedObjects.swap(newSelectedObjects);
        return (numUngrouped>0);
    }

    EventResponse ungroupObjects(EventPtr ev) {
        Task::AbsTime now(Task::AbsTime::now());
        doUngroupObjects(now);
        return EventResponse::nop();
    }

    EventResponse enterObject(EventPtr ev) {
        Task::AbsTime now(Task::AbsTime::now());
        if (mSelectedObjects.size() != 1) {
            return EventResponse::nop();
        }
        Entity *parentEnt = NULL;
        for (SelectedObjectSet::iterator iter = mSelectedObjects.begin();
                iter != mSelectedObjects.end(); ++iter) {
            parentEnt = mParent->getEntity((*iter)->getObjectReference());
        }
        if (parentEnt) {
            SelectedObjectSet newSelectedObjects;
            bool hasSubObjects = false;
            for (SubObjectIterator subIter (parentEnt); !subIter.end(); ++subIter) {
                hasSubObjects = true;
                Entity *ent = *subIter;
                newSelectedObjects.insert(ent->getProxyPtr());
                ent->setSelected(true);
            }
            if (hasSubObjects) {
                mSelectedObjects.swap(newSelectedObjects);
                mCurrentGroup = parentEnt->id();
            }
        }
        return EventResponse::nop();
    }

    EventResponse leaveObject(EventPtr ev) {
        Task::AbsTime now(Task::AbsTime::now());
        for (SelectedObjectSet::iterator iter = mSelectedObjects.begin();
                iter != mSelectedObjects.end(); ++iter) {
            Entity *selent = mParent->getEntity((*iter)->getObjectReference());
            if (selent) {
                selent->setSelected(false);
            }
        }
        mSelectedObjects.clear();
        Entity *ent = mParent->getEntity(mCurrentGroup);
        if (ent) {
            mCurrentGroup = ent->getProxy().getParent();
            Entity *parentEnt = mParent->getEntity(mCurrentGroup);
            if (parentEnt) {
                mSelectedObjects.insert(parentEnt->getProxyPtr());
            }
        }
        else {
            mCurrentGroup = SpaceObjectReference::null();
        }
        return EventResponse::nop();
    }

    EventResponse createLight(EventPtr ev) {
        float WORLD_SCALE = mParent->mInputManager->mWorldScale->as<float>();
        Task::AbsTime now(Task::AbsTime::now());
        CameraEntity *camera = mParent->mPrimaryCamera;
        SpaceObjectReference newId = SpaceObjectReference(camera->id().space(), ObjectReference(UUID::random()));
        ProxyManager *proxyMgr = camera->getProxy().getProxyManager();
        Location loc (camera->getProxy().globalLocation(now));
        loc.setPosition(loc.getPosition() + Vector3d(direction(loc.getOrientation()))*WORLD_SCALE);
        loc.setOrientation(Quaternion(0.886995, 0.000000, -0.461779, 0.000000, Quaternion::WXYZ()));

        std::tr1::shared_ptr<ProxyLightObject> newLightObject (new ProxyLightObject(proxyMgr, newId));
        proxyMgr->createObject(newLightObject);
        {
            LightInfo li;
            li.setLightDiffuseColor(Color(0.976471, 0.992157, 0.733333));
            li.setLightAmbientColor(Color(.24,.25,.18));
            li.setLightSpecularColor(Color(0,0,0));
            li.setLightShadowColor(Color(0,0,0));
            li.setLightPower(1.0);
            li.setLightRange(75);
            li.setLightFalloff(1,0,0.03);
            li.setLightSpotlightCone(30,40,1);
            li.setCastsShadow(true);
            /* set li according to some sample light in the scene file! */
            newLightObject->update(li);
        }

        Entity *parentent = mParent->getEntity(mCurrentGroup);
        if (parentent) {
            Location localLoc = loc.toLocal(parentent->getProxy().globalLocation(now));
            newLightObject->setParent(parentent->getProxyPtr(), now, loc, localLoc);
            newLightObject->resetPositionVelocity(now, localLoc);
        }
        else {
            newLightObject->resetPositionVelocity(now, loc);
        }
        mSelectedObjects.clear();
        mSelectedObjects.insert(newLightObject);
        Entity *ent = mParent->getEntity(newId);
        if (ent) {
            ent->setSelected(true);
        }
        return EventResponse::nop();
    }

    EventResponse moveHandler(EventPtr ev) {
        float angSpeed;
        Vector3f velocity;
        Vector3f yawAxis;
        float WORLD_SCALE = mParent->mInputManager->mWorldScale->as<float>();
        Task::AbsTime now(Task::AbsTime::now());
        std::tr1::shared_ptr<ButtonEvent> buttonev (
            std::tr1::dynamic_pointer_cast<ButtonEvent>(ev));
        if (!buttonev) {
            return EventResponse::nop();
        }
        float amount = buttonev->mPressed?1:0;
        static float camSpeed = 1.0f;

        CameraEntity *cam = mParent->mPrimaryCamera;
        ProxyPositionObjectPtr camProxy = cam->getProxyPtr();
        ProxyPositionObjectPtr parentProxy;
        while (parentProxy = camProxy->getParentProxy()) {
            camProxy = parentProxy;
        }
        Location loc = camProxy->extrapolateLocation(now);
        const Quaternion &orient = loc.getOrientation();

        switch (buttonev->mButton) {
        case SDL_SCANCODE_S:
            amount*=-1;
        case SDL_SCANCODE_W:
            amount *= camSpeed;
            amount *= WORLD_SCALE;
            std::cout << "dbm debug setting camera velocity to " << direction(orient)*amount << std::endl;
            loc.setVelocity(direction(orient)*amount);
            loc.setAngularSpeed(0);
            break;
        case SDL_SCANCODE_A:
            amount*=-1;
        case SDL_SCANCODE_D:
            amount *= camSpeed;
            amount *= WORLD_SCALE;
            loc.setVelocity(orient.xAxis()*amount);
            loc.setAngularSpeed(0);
            break;
        case SDL_SCANCODE_DOWN:
            amount*=-1;
        case SDL_SCANCODE_UP:
            angSpeed = 0;
            velocity = Vector3f(0,0,0);
            /// fwd, but also if CTL: pan left, ALT: pan right
            /// SHIFT: pan up, SHIFT+CTL: pan down
            if (!mParent->mInputManager->isModifierDown(InputDevice::MOD_SHIFT)) {
                if ( (mParent->mInputManager->isModifierDown(InputDevice::MOD_CTRL)) ||
                        (mParent->mInputManager->isModifierDown(InputDevice::MOD_ALT)) ) {
                    int oldamt = amount;
                    if (mParent->mInputManager->isModifierDown(InputDevice::MOD_ALT)) {
                        amount *= -1;
                    }
                    /// AngularSpeed needs a relative axis, so compute the global Y axis (yawAxis) in local frame
                    double p, r, y;
                    quat2Euler(loc.getOrientation(), p, r, y);
                    yawAxis.x = 0;
                    yawAxis.y = std::cos(p*DEG2RAD);
                    yawAxis.z = -std::sin(p*DEG2RAD);
                    loc.setAxisOfRotation(yawAxis);
                    angSpeed=buttonev->mPressed?amount:0;
                    amount = oldamt;
                }
                else {
                    amount *= camSpeed;
                }
            }
            if (mParent->mInputManager->isModifierDown(InputDevice::MOD_SHIFT)) {
                if (mParent->mInputManager->isModifierDown(InputDevice::MOD_CTRL)) {
                    amount *= -1;
                }
                loc.setAxisOfRotation(Vector3f(1,0,0));
                angSpeed=buttonev->mPressed?amount:0;
            }
            else {
                amount *= WORLD_SCALE;
                velocity = direction(orient)*amount;
            }
            loc.setAngularSpeed(angSpeed);
            loc.setVelocity(velocity);
            break;
        case SDL_SCANCODE_PAGEDOWN:
            amount*=-1;
        case SDL_SCANCODE_PAGEUP:
            amount *= 0.25;
            amount *= WORLD_SCALE;
            loc.setVelocity(Vector3f(0,1,0)*amount);
            loc.setAngularSpeed(0);
            break;
        case SDL_SCANCODE_RIGHT:
            amount*=-1;
        case SDL_SCANCODE_LEFT:
            /// default: strafe.  SHIFT: pan (turn your head)
            if (mParent->mInputManager->isModifierDown(InputDevice::MOD_SHIFT)) {
                /// AngularSpeed needs a relative axis, so compute the global Y axis (yawAxis) in local frame
                double p, r, y;
                quat2Euler(loc.getOrientation(), p, r, y);
                yawAxis.x = 0;
                yawAxis.y = std::cos(p*DEG2RAD);
                yawAxis.z = -std::sin(p*DEG2RAD);
                loc.setAxisOfRotation(yawAxis);
                loc.setAngularSpeed(buttonev->mPressed?amount:0);
                loc.setVelocity(Vector3f(0,0,0));
                break;
            }
            else {
                amount *= WORLD_SCALE*-.25*camSpeed;
                loc.setVelocity(orient.xAxis()*amount);
                loc.setAngularSpeed(0);
                break;
            }
        case SDL_SCANCODE_1:
            camSpeed=0.25;
            break;
        case SDL_SCANCODE_2:
            camSpeed=1.0;
            break;
        case SDL_SCANCODE_3:
            camSpeed=5.0;
            break;
        default:
            break;
        }
        std::cout << "dbm debug setting camera velocity(2) to " << loc << std::endl;
        camProxy->setPositionVelocity(now, loc);
        return EventResponse::nop();
    }

    EventResponse import(EventPtr ev) {
        std::cout << "input path name for import: " << std::endl;
        std::string filename;
        // a bit of a cludge right now, type name into console.
        fflush(stdin);
        while (!feof(stdin)) {
            int c = fgetc(stdin);
            if (c == '\r') {
                c = fgetc(stdin);
            }
            if (c=='\n') {
                break;
            }
            if (c=='\033' || c <= 0) {
                std::cout << "<escape>\n";
                return EventResponse::nop();
            }
            std::cout << (unsigned char)c;
            filename += (unsigned char)c;
        }
        std::cout << '\n';
        std::vector<std::string> files;
        files.push_back(filename);
        mParent->mInputManager->filesDropped(files);
        return EventResponse::cancel();
    }

    set<string> saveSceneNames;
    map<ProxyMeshObject*,string> entityNames;
    
    EventResponse saveScene(EventPtr ev) {
        saveSceneNames.clear();
        entityNames.clear();
        std::cout << "saving new scene as scene_new.csv: " << std::endl;
        FILE *output = fopen("scene_new.csv", "wt");
        if (!output) {
            perror("Failed to open scene_new.csv");
            return EventResponse::cancel();
        }
        fprintf(output, "objtype,subtype,name,parent,");
        fprintf(output, "pos_x,pos_y,pos_z,orient_x,orient_y,orient_z,orient_w,scale_x,scale_y,scale_z,hull_x,hull_y,hull_z,");
        fprintf(output, "density,friction,bounce,colMask,colMsg,meshURI,diffuse_x,diffuse_y,diffuse_z,ambient,");
        fprintf(output, "specular_x,specular_y,specular_z,shadowpower,");
        fprintf(output, "range,constfall,linearfall,quadfall,cone_in,cone_out,power,cone_fall,shadow\n");
        OgreSystem::SceneEntitiesMap::const_iterator iter;
        vector<Entity*> entlist;
        entlist.clear();
        for (iter = mParent->mSceneEntities.begin(); iter != mParent->mSceneEntities.end(); ++iter) {
            entlist.push_back(iter->second);
        }
        std::sort(entlist.begin(), entlist.end(), compareEntity);
        for (unsigned int i=0; i<entlist.size(); i++)
            dumpObject(output, entlist[i]);
        fclose(output);
        return EventResponse::cancel();
    }

    bool quat2Euler(Quaternion q, double& pitch, double& roll, double& yaw) {
        /// note that in the 'gymbal lock' situation, we will get nan's for pitch.
        /// for now, in that case we should revert to quaternion
        double q1,q2,q3,q0;
        q2=q.x;
        q3=q.y;
        q1=q.z;
        q0=q.w;
        roll = std::atan2((2*((q0*q1)+(q2*q3))), (1-(2*(std::pow(q1,2.0)+std::pow(q2,2.0)))));
        pitch = std::asin((2*((q0*q2)-(q3*q1))));
        yaw = std::atan2((2*((q0*q3)+(q1*q2))), (1-(2*(std::pow(q2,2.0)+std::pow(q3,2.0)))));
        pitch /= DEG2RAD;
        roll /= DEG2RAD;
        yaw /= DEG2RAD;
        if (std::abs(pitch) > 89.0) {
            return false;
        }
        return true;
    }
    
    string physicalName(ProxyMeshObject *obj) {
        map<ProxyMeshObject*,string>::iterator iter = entityNames.find(obj);
        if (iter != entityNames.end()) {
            return iter->second;
        }
        std::string name = obj->getPhysical().name;
        if (name.empty()) {
            name = obj->getMesh().filename();
            if (name.size() > 5) {
                name.resize(name.size()-5);
            } else {
                name = "group";
            }
        }
        int basesize = name.size();
        int count = 1;
        while (saveSceneNames.count(name)) {
            name.resize(basesize);
            std::ostringstream os;
            os << name << "." << count;
            name = os.str();
            count++;
        }
        saveSceneNames.insert(name);
        entityNames.insert(map<ProxyMeshObject*,string>::value_type(obj, name));
        return name;
    }
    void dumpObject(FILE* fp, Entity* e) {
        Task::AbsTime now = Task::AbsTime::now();
        ProxyPositionObject *pp = e->getProxyPtr().get();
        Location loc = pp->globalLocation(now);
        ProxyCameraObject* camera = dynamic_cast<ProxyCameraObject*>(pp);
        ProxyLightObject* light = dynamic_cast<ProxyLightObject*>(pp);
        ProxyMeshObject* mesh = dynamic_cast<ProxyMeshObject*>(pp);

        double x,y,z;
        std::string w("");
        /// if feasible, use Eulers: (not feasible == potential gymbal confusion)
        if (!quat2Euler(loc.getOrientation(), x, z, y)) {
            x=loc.getOrientation().x;
            y=loc.getOrientation().y;
            z=loc.getOrientation().z;
            std::stringstream temp;
            temp << loc.getOrientation().w;
            w = temp.str();
        }
        string parent;
        ProxyPositionObjectPtr parentObj = pp->getParentProxy();
        if (parentObj) {
            ProxyMeshObject *parentMesh = dynamic_cast<ProxyMeshObject*>(parentObj.get());
            if (parentMesh) {
                parent = physicalName(parentMesh);
            }
        }
        if (light) {
            const char *typestr = "directional";
            const LightInfo &linfo = light->getLastLightInfo();
            if (linfo.mType == LightInfo::POINT) {
                typestr = "point";
            }
            if (linfo.mType == LightInfo::SPOTLIGHT) {
                typestr = "spotlight";
            }
            float32 ambientPower, shadowPower;
            ambientPower = LightEntity::computeClosestPower(linfo.mDiffuseColor, linfo.mAmbientColor, linfo.mPower);
            shadowPower = LightEntity::computeClosestPower(linfo.mSpecularColor, linfo.mShadowColor,  linfo.mPower);
            fprintf(fp, "light,%s,,%s,%f,%f,%f,%f,%f,%f,%s,,,,,,,,,,,,,",typestr,parent.c_str(),
                    loc.getPosition().x,loc.getPosition().y,loc.getPosition().z,
                    x,y,z,w.c_str());

            fprintf(fp, "%f,%f,%f,%f,%f,%f,%f,%f,%lf,%f,%f,%f,%f,%f,%f,%f,%d\n",
                    linfo.mDiffuseColor.x,linfo.mDiffuseColor.y,linfo.mDiffuseColor.z,ambientPower,
                    linfo.mSpecularColor.x,linfo.mSpecularColor.y,linfo.mSpecularColor.z,shadowPower,
                    linfo.mLightRange,linfo.mConstantFalloff,linfo.mLinearFalloff,linfo.mQuadraticFalloff,
                    linfo.mConeInnerRadians,linfo.mConeOuterRadians,linfo.mPower,linfo.mConeFalloff,
                    (int)linfo.mCastsShadow);
        }
        else if (mesh) {
            URI uri = mesh->getMesh();
            std::string uristr = uri.toString();
            if (uri.proto().empty()) {
                uristr = "";
            }
            const physicalParameters &phys = mesh->getPhysical();
            std::string subtype;
            switch (phys.mode) {
            case bulletObj::Disabled:
                subtype="graphiconly";
                break;
            case bulletObj::Static:
                subtype="staticmesh";
                break;
            case bulletObj::DynamicBox:
                subtype="dynamicbox";
                break;
            case bulletObj::DynamicSphere:
                subtype="dynamicsphere";
                break;
            case bulletObj::DynamicCylinder:
                subtype="dynamiccylinder";
                break;
            default:
                std::cout << "unknown physical mode! " << phys.mode << std::endl;
            }
            std::string name = physicalName(mesh);
            fprintf(fp, "mesh,%s,%s,%s,%f,%f,%f,%f,%f,%f,%s,",subtype.c_str(),name.c_str(),parent.c_str(),
                    loc.getPosition().x,loc.getPosition().y,loc.getPosition().z,
                    x,y,z,w.c_str());

            fprintf(fp, "%f,%f,%f,%f,%f,%f,%f,%f,%f,%d,%d,%s\n",
                    mesh->getScale().x,mesh->getScale().y,mesh->getScale().z,
                    phys.hull.x, phys.hull.y, phys.hull.z,
                    phys.density, phys.friction, phys.bounce, phys.colMask, phys.colMsg, uristr.c_str());
        }
        else if (camera) {
            fprintf(fp, "camera,,,%s,%f,%f,%f,%f,%f,%f,%s\n",parent.c_str(),
                    loc.getPosition().x,loc.getPosition().y,loc.getPosition().z,
                    x,y,z,w.c_str());
        }
        else {
            fprintf(fp, "#unknown object type in dumpObject\n");
        }
    }

    ///////////////// DEVICE FUNCTIONS ////////////////

    SubscriptionId registerAxisListener(const InputDevicePtr &dev,
                                        EventResponse(MouseHandler::*func)(EventPtr),
                                        int axis) {
        Task::IdPair eventId (AxisEvent::getEventId(),
                              AxisEvent::getSecondaryId(dev, axis));
        SubscriptionId subId = mParent->mInputManager->subscribeId(eventId,
                               std::tr1::bind(func, this, _1));
        mEvents.push_back(subId);
        mDeviceSubscriptions.insert(DeviceSubMap::value_type(&*dev, subId));
        return subId;
    }

    SubscriptionId registerButtonListener(const InputDevicePtr &dev,
                                          EventResponse(MouseHandler::*func)(EventPtr),
                                          int button, bool released=false, InputDevice::Modifier mod=0) {
        Task::IdPair eventId (released?ButtonReleased::getEventId():ButtonPressed::getEventId(),
                              ButtonEvent::getSecondaryId(button, mod, dev));
        SubscriptionId subId = mParent->mInputManager->subscribeId(eventId,
                               std::tr1::bind(func, this, _1));
        mEvents.push_back(subId);
        mDeviceSubscriptions.insert(DeviceSubMap::value_type(&*dev, subId));
        return subId;
    }

    EventResponse setDragMode(EventPtr evbase) {
        std::tr1::shared_ptr<ButtonEvent> ev (
            std::tr1::dynamic_pointer_cast<ButtonEvent>(evbase));
        if (!ev) {
            return EventResponse::nop();
        }
        switch (ev->mButton) {
        case SDL_SCANCODE_Q:
            mDragAction[1] = 0;
            clearSelection();
            break;
        case SDL_SCANCODE_W:
            mDragAction[1] = DragActionRegistry::get("moveObject");
            break;
        case SDL_SCANCODE_E:
            mDragAction[1] = DragActionRegistry::get("rotateObject");
            break;
        case SDL_SCANCODE_R:
            mDragAction[1] = DragActionRegistry::get("scaleObject");
            break;
        case SDL_SCANCODE_T:
            mDragAction[1] = DragActionRegistry::get("rotateCamera");
            break;
        case SDL_SCANCODE_Y:
            mDragAction[1] = DragActionRegistry::get("panCamera");
            break;
        case SDL_SCANCODE_P:
            /// not a dragmode, but whatever -- it does what we want
            mParent->mDisablePhysics = !mParent->mDisablePhysics;
            break;
        }
        return EventResponse::nop();
    }

    EventResponse deviceListener(EventPtr evbase) {
        std::tr1::shared_ptr<InputDeviceEvent> ev (std::tr1::dynamic_pointer_cast<InputDeviceEvent>(evbase));
        if (!ev) {
            return EventResponse::nop();
        }
        switch (ev->mType) {
        case InputDeviceEvent::ADDED:
            if (!!(std::tr1::dynamic_pointer_cast<SDLMouse>(ev->mDevice))) {
                registerAxisListener(ev->mDevice, &MouseHandler::wheelListener, SDLMouse::WHEELX);
                registerAxisListener(ev->mDevice, &MouseHandler::wheelListener, SDLMouse::WHEELY);
            }
            if (!!(std::tr1::dynamic_pointer_cast<SDLKeyboard>(ev->mDevice))) {
                registerButtonListener(ev->mDevice, &MouseHandler::moveHandler, SDL_SCANCODE_1);
                registerButtonListener(ev->mDevice, &MouseHandler::moveHandler, SDL_SCANCODE_2);
                registerButtonListener(ev->mDevice, &MouseHandler::moveHandler, SDL_SCANCODE_3);
                registerButtonListener(ev->mDevice, &MouseHandler::groupObjects, SDL_SCANCODE_G);
                registerButtonListener(ev->mDevice, &MouseHandler::ungroupObjects, SDL_SCANCODE_G,false,InputDevice::MOD_ALT);
                registerButtonListener(ev->mDevice, &MouseHandler::deleteObjects, SDL_SCANCODE_DELETE);
                registerButtonListener(ev->mDevice, &MouseHandler::deleteObjects, SDL_SCANCODE_KP_PERIOD); // Del
                registerButtonListener(ev->mDevice, &MouseHandler::cloneObjects, SDL_SCANCODE_V,false,InputDevice::MOD_CTRL);
                registerButtonListener(ev->mDevice, &MouseHandler::cloneObjects, SDL_SCANCODE_D);
                registerButtonListener(ev->mDevice, &MouseHandler::enterObject, SDL_SCANCODE_KP_ENTER);
                registerButtonListener(ev->mDevice, &MouseHandler::enterObject, SDL_SCANCODE_RETURN);
                registerButtonListener(ev->mDevice, &MouseHandler::leaveObject, SDL_SCANCODE_KP_0);
                registerButtonListener(ev->mDevice, &MouseHandler::leaveObject, SDL_SCANCODE_ESCAPE);
                registerButtonListener(ev->mDevice, &MouseHandler::createLight, SDL_SCANCODE_B);

                registerButtonListener(ev->mDevice, &MouseHandler::moveHandler, SDL_SCANCODE_PAGEUP);
                registerButtonListener(ev->mDevice, &MouseHandler::moveHandler, SDL_SCANCODE_PAGEUP,true);
                registerButtonListener(ev->mDevice, &MouseHandler::moveHandler, SDL_SCANCODE_PAGEDOWN);
                registerButtonListener(ev->mDevice, &MouseHandler::moveHandler, SDL_SCANCODE_PAGEDOWN,true);
                registerButtonListener(ev->mDevice, &MouseHandler::moveHandler, SDL_SCANCODE_W,false, InputDevice::MOD_SHIFT);
                registerButtonListener(ev->mDevice, &MouseHandler::moveHandler, SDL_SCANCODE_A,false, InputDevice::MOD_SHIFT);
                registerButtonListener(ev->mDevice, &MouseHandler::moveHandler, SDL_SCANCODE_S,false, InputDevice::MOD_SHIFT);
                registerButtonListener(ev->mDevice, &MouseHandler::moveHandler, SDL_SCANCODE_D,false, InputDevice::MOD_SHIFT);
                registerButtonListener(ev->mDevice, &MouseHandler::moveHandler, SDL_SCANCODE_UP);
                registerButtonListener(ev->mDevice, &MouseHandler::moveHandler, SDL_SCANCODE_UP, true, InputDevice::MOD_SHIFT);
                registerButtonListener(ev->mDevice, &MouseHandler::moveHandler, SDL_SCANCODE_UP, false, InputDevice::MOD_SHIFT);
                registerButtonListener(ev->mDevice, &MouseHandler::moveHandler, SDL_SCANCODE_UP, true,
                                       InputDevice::MOD_SHIFT|InputDevice::MOD_CTRL);
                registerButtonListener(ev->mDevice, &MouseHandler::moveHandler, SDL_SCANCODE_UP, false,
                                       InputDevice::MOD_SHIFT|InputDevice::MOD_CTRL);
                registerButtonListener(ev->mDevice, &MouseHandler::moveHandler, SDL_SCANCODE_UP, true, InputDevice::MOD_ALT);
                registerButtonListener(ev->mDevice, &MouseHandler::moveHandler, SDL_SCANCODE_UP, false, InputDevice::MOD_ALT);
                registerButtonListener(ev->mDevice, &MouseHandler::moveHandler, SDL_SCANCODE_UP, true, InputDevice::MOD_CTRL);
                registerButtonListener(ev->mDevice, &MouseHandler::moveHandler, SDL_SCANCODE_UP, false, InputDevice::MOD_CTRL);
                registerButtonListener(ev->mDevice, &MouseHandler::moveHandler, SDL_SCANCODE_DOWN);
                registerButtonListener(ev->mDevice, &MouseHandler::moveHandler, SDL_SCANCODE_DOWN, true, InputDevice::MOD_SHIFT);
                registerButtonListener(ev->mDevice, &MouseHandler::moveHandler, SDL_SCANCODE_DOWN, false, InputDevice::MOD_SHIFT);
                registerButtonListener(ev->mDevice, &MouseHandler::moveHandler, SDL_SCANCODE_LEFT);
                registerButtonListener(ev->mDevice, &MouseHandler::moveHandler, SDL_SCANCODE_LEFT, true, InputDevice::MOD_SHIFT);
                registerButtonListener(ev->mDevice, &MouseHandler::moveHandler, SDL_SCANCODE_LEFT, false, InputDevice::MOD_SHIFT);
                registerButtonListener(ev->mDevice, &MouseHandler::moveHandler, SDL_SCANCODE_RIGHT);
                registerButtonListener(ev->mDevice, &MouseHandler::moveHandler, SDL_SCANCODE_RIGHT, true, InputDevice::MOD_SHIFT);
                registerButtonListener(ev->mDevice, &MouseHandler::moveHandler, SDL_SCANCODE_RIGHT, false, InputDevice::MOD_SHIFT);
                registerButtonListener(ev->mDevice, &MouseHandler::moveHandler, SDL_SCANCODE_W,true, InputDevice::MOD_SHIFT);
                registerButtonListener(ev->mDevice, &MouseHandler::moveHandler, SDL_SCANCODE_A,true, InputDevice::MOD_SHIFT);
                registerButtonListener(ev->mDevice, &MouseHandler::moveHandler, SDL_SCANCODE_S,true, InputDevice::MOD_SHIFT);
                registerButtonListener(ev->mDevice, &MouseHandler::moveHandler, SDL_SCANCODE_D,true, InputDevice::MOD_SHIFT);
                registerButtonListener(ev->mDevice, &MouseHandler::moveHandler, SDL_SCANCODE_UP,true);
                registerButtonListener(ev->mDevice, &MouseHandler::moveHandler, SDL_SCANCODE_DOWN,true);
                registerButtonListener(ev->mDevice, &MouseHandler::moveHandler, SDL_SCANCODE_LEFT,true);
                registerButtonListener(ev->mDevice, &MouseHandler::moveHandler, SDL_SCANCODE_RIGHT,true);
                registerButtonListener(ev->mDevice, &MouseHandler::import, SDL_SCANCODE_O, false, InputDevice::MOD_CTRL);
                registerButtonListener(ev->mDevice, &MouseHandler::saveScene, SDL_SCANCODE_S, false, InputDevice::MOD_CTRL);

                registerButtonListener(ev->mDevice, &MouseHandler::setDragMode, SDL_SCANCODE_Q);
                registerButtonListener(ev->mDevice, &MouseHandler::setDragMode, SDL_SCANCODE_W);
                registerButtonListener(ev->mDevice, &MouseHandler::setDragMode, SDL_SCANCODE_E);
                registerButtonListener(ev->mDevice, &MouseHandler::setDragMode, SDL_SCANCODE_R);
                registerButtonListener(ev->mDevice, &MouseHandler::setDragMode, SDL_SCANCODE_T);
                registerButtonListener(ev->mDevice, &MouseHandler::setDragMode, SDL_SCANCODE_Y);
                registerButtonListener(ev->mDevice, &MouseHandler::setDragMode, SDL_SCANCODE_P);
            }
            break;
        case InputDeviceEvent::REMOVED: {
            DeviceSubMap::iterator iter;
            while ((iter = mDeviceSubscriptions.find(&*ev->mDevice))!=mDeviceSubscriptions.end()) {
                mParent->mInputManager->unsubscribe((*iter).second);
                mDeviceSubscriptions.erase(iter);
            }
        }
        break;
        }
        return EventResponse::nop();
    }

    EventResponse doDrag(EventPtr evbase) {
        MouseDragEventPtr ev (std::tr1::dynamic_pointer_cast<MouseDragEvent>(evbase));
        if (!ev) {
            return EventResponse::nop();
        }
        ActiveDrag * &drag = mActiveDrag[ev->mButton];
        if (ev->mType == MouseDragEvent::START) {
            if (drag) {
                delete drag;
            }
            DragStartInfo info = {
                /*.sys = */ mParent,
                /*.camera = */ mParent->mPrimaryCamera, // for now...
                /*.objects = */ mSelectedObjects,
                /*.ev = */ ev
            };
            if (mDragAction[ev->mButton]) {
                drag = mDragAction[ev->mButton](info);
            }
        }
        if (drag) {
            drag->mouseMoved(ev);

            if (ev->mType == MouseDragEvent::END) {
                delete drag;
                drag = 0;
            }
        }
        return EventResponse::nop();
    }

// .....................

// SCROLL WHEEL: up/down = move object closer/farther. left/right = rotate object about Y axis.
// holding down middle button up/down = move object closer/farther, left/right = rotate object about Y axis.

// Accuracy: relative versus absolute mode; exponential decay versus pixels.

public:
    MouseHandler(OgreSystem *parent) :
            mParent(parent),
            mCurrentGroup(SpaceObjectReference::null()),
            mStartTime(Task::AbsTime::now()),
            mWhichRayObject(0) {
        mLastHitCount=0;
        mLastHitX=0;
        mLastHitY=0;

        mEvents.push_back(mParent->mInputManager->registerDeviceListener(
                              std::tr1::bind(&MouseHandler::deviceListener, this, _1)));

        mEvents.push_back(mParent->mInputManager->subscribeId(
                              MouseDragEvent::getEventId(),
                              std::tr1::bind(&MouseHandler::doDrag, this, _1)));
        mDragAction[1] = 0;
        mDragAction[2] = DragActionRegistry::get("panCamera");
        mDragAction[3] = DragActionRegistry::get("rotateCamera");
        mDragAction[4] = DragActionRegistry::get("zoomCamera");

        mEvents.push_back(mParent->mInputManager->subscribeId(
                              IdPair(MouseClickEvent::getEventId(),
                                     MouseDownEvent::getSecondaryId(1)),
                              std::tr1::bind(&MouseHandler::selectObject, this, _1, 1)));
        mEvents.push_back(mParent->mInputManager->subscribeId(
                              IdPair(MouseClickEvent::getEventId(),
                                     MouseDownEvent::getSecondaryId(3)),
                              std::tr1::bind(&MouseHandler::selectObject, this, _1, -1)));
    }
    ~MouseHandler() {
        for (std::vector<SubscriptionId>::const_iterator iter = mEvents.begin();
                iter != mEvents.end();
                ++iter) {
            mParent->mInputManager->unsubscribe(*iter);
        }
    }
    void setParentGroupAndClear(const SpaceObjectReference &id) {
        clearSelection();
        mCurrentGroup = id;
    }
    const SpaceObjectReference &getParentGroup() const {
        return mCurrentGroup;
    }
    void addToSelection(const ProxyPositionObjectPtr &obj) {
        mSelectedObjects.insert(obj);
    }
};

void OgreSystem::allocMouseHandler() {
    mMouseHandler = new MouseHandler(this);
}
void OgreSystem::destroyMouseHandler() {
    if (mMouseHandler) {
        delete mMouseHandler;
    }
}

void OgreSystem::selectObject(Entity *obj, bool replace) {
    if (replace) {
        mMouseHandler->setParentGroupAndClear(obj->getProxy().getParent());
    }
    if (mMouseHandler->getParentGroup() == obj->getProxy().getParent()) {
        mMouseHandler->addToSelection(obj->getProxyPtr());
        obj->setSelected(true);
    }
}

void OgreSystem::hiliteSelection() {
    if (mMouseHandler)
        mMouseHandler->hiliteSelection();
}

}
}
