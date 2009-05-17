/*  Sirikata Input Plugin -- plugins/input
 *  InputDevice.hpp
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

#ifndef SIRIKATA_INPUT_InputDevice_HPP__
#define SIRIKATA_INPUT_InputDevice_HPP__

#include <util/Platform.hpp>
#include <util/Time.hpp>
#include <task/EventManager.hpp>
#include <task/Event.hpp>

namespace Sirikata {
namespace Input {

/*

==== AVATAR IS FOCUSED ====

Walk Forward/Backward: <Axis Type>
    Mouse X [Edit] "M/0/x"
    Mouse Y [Edit] "M/0/y"
    Joystick Axis 0 [Edit] "J/0/0"
    Joystick Axis 1 [Edit] "J/0/1"
    Joystick Axis 2 [Edit] "J/0/2"
    Tablet/Touchscreen [Edit] "MA/0/x"
    Up/Down arrow keys [Edit] "KA/0"
    [Keyboard/Hatswitch]
    [New Tablet Mode]
Walk Left/Right: <A/D>
Turn Left/Right: <Mouse X>
Turn Up/Down: <W/S>

Jump: [Space]
Select: [Type or pick key]
    Mouse up [Edit]
    [Make new analog Hatswitch / key from axis.]


==== SCENE HAS FOCUS (or null) ====

         Absolute pointer 0: [Mouse 1 Cursor]
         Absolute pointer 1: [Touchpad]
         Absolute pointer 2: [Joystick Axis 1+2]
         Absolute pointer 3: [Keyboard Axis "Up/Down arrow keys"/"Left/Right arrow keys"]
         Return from UI: [ESCAPE]

         Key bindings: 
[Not implemented, but you can bind buttons to individual keys if you want]

==== FLASH GAME IS FOCUSED ====
Choose two analog inputs and one keyboard/joystick->key bindings

*/

struct AxisValue {
    float value;
    float get01() {
        return (value + 1)/2.;
    }
    float getCentered() {
        return value;
    }
    static AxisValue fromCentered(float val) {
        AxisValue ret = {val};
        return ret;
    }
    static AxisValue from01(float val) {
        AxisValue ret = {(val-0.5)*2.};
        return ret;
    }
    static AxisValue null() {
        return fromCentered(0.0);
    }

    bool isNegative() const {
        return value < 0;
    }
    bool isPositive() const { 
        return value > 0;
    }

    void clip() {
        if (value > 1.0) {
            value = 1.0;
        }
        if (value < -1.0) {
            value = -1.0;
        }
    }
    bool operator == (const AxisValue &other) const {
        return value == other.value;
    }
    bool operator != (const AxisValue &other) const {
        return value != other.value;
    }

};

class InputDevice;
typedef std::tr1::shared_ptr<InputDevice> InputDevicePtr;
typedef std::tr1::weak_ptr<InputDevice> InputDeviceWPtr;

class InputDevice {
public:
    typedef uint32 Modifier;
protected:
    std::string mName;

    typedef std::tr1::unordered_map<unsigned int, Modifier> ButtonSet;
    typedef std::vector<AxisValue> AxisVector;

    ButtonSet buttonState;
    AxisVector axisState;
public:
    const std::string &getName() const {
        return mName;
    }
    void setName(const std::string &newName) {
        mName = newName;
    }

    virtual ~InputDevice() {}

    virtual std::string getButtonName(unsigned int button) const = 0;
    virtual int getNumButtons() const = 0;

    virtual std::string getAxisName(unsigned int axis) const = 0;
    virtual unsigned int getNumAxes() const = 0;

    bool buttonChanged(unsigned int button, bool newState, Modifier &mod);
    bool axisChanged(unsigned int axis, AxisValue newValue);
    
    bool fireButton(const InputDevicePtr &thisptr,
                    Task::GenEventManager *em,
                    unsigned int button, bool newState, Modifier mod = 0);
    bool fireAxis(const InputDevicePtr &thisptr, 
                  Task::GenEventManager *em, 
                  unsigned int axis, AxisValue newState);

    inline AxisValue getAxis(unsigned int axis) const {
        if (axisState.size() <= axis) {
            return AxisValue::null();
        }
        return axisState[axis];
    }
    inline bool getButton(unsigned int button, Modifier mod) const {
        ButtonSet::const_iterator iter = buttonState.find(button);
        if (iter != buttonState.end()) {
            return (*iter).second == mod;
        } else {
            return false;
        }
    }
    inline const Modifier *getButton(unsigned int button) const {
        ButtonSet::const_iterator iter = buttonState.find(button);
        if (iter != buttonState.end()) {
            return &((*iter).second);
        } else {
            return NULL;
        }
    }
};


/** Mostly for hatswitches or d-pad's on a joystick. */
class AxisToButton : public InputDevice {
    struct HatButton {
        AxisValue mLow;
        AxisValue mHigh;
    };
    std::vector <HatButton> mButtons;

    InputDevicePtr mParentDevice;
    int mParentAxis;

public:
    AxisToButton (const std::string &name, const InputDevicePtr &parent, int parentAxis) 
        : mParentDevice(parent), mParentAxis(parentAxis) {
        setName(name);
    }

    virtual std::string getButtonName(unsigned int num) const {
        std::ostringstream os ("[" + mParentDevice->getAxisName(mParentAxis) + " ");
        const HatButton & button = mButtons[num];
        if (button.mLow.isNegative() && button.mHigh.isPositive()) {
            os << "Centered";
        } else {
            bool lessZero = button.mHigh.isNegative();
            AxisValue extreme = button.mHigh;
            if (lessZero) {
                extreme = button.mLow;
            }
            os << ((int)(extreme.getCentered()*100)) << "%";
        }
        os << "]";
        return os.str();
    }

    virtual int getNumButtons() const {
        return mButtons.size();
    }
    virtual unsigned int getNumAxes() const {
        return 0;
    }
    virtual std::string getNumAxisName(unsigned int i) const {
        return std::string();
    }

    void addButton(AxisValue low, AxisValue high) {
        HatButton hb = {low, high};
        mButtons.push_back(hb);
    }

};

/** Make arrow keys on a keyboard emulate a mouse/axis. */
class ButtonToAxis : public InputDevice {
    typedef std::map <int, float> AxisMap;
    AxisMap mAxisPoints;
    InputDevicePtr mParentDevice;
    int mParentButton;
public:
    ButtonToAxis (const std::string &name, const InputDevicePtr &parent, int parentButton) 
        : mParentDevice(parent), mParentButton(parentButton) {
        setName(name);
    }

    virtual std::string getAxisName(unsigned int axis) {
        std::string os = "[" + mParentDevice->getName() + " ";
        bool first = true;
        for (AxisMap::const_iterator iter = mAxisPoints.begin(); iter != mAxisPoints.end(); ++iter) {
            if (!first) {
                os += '/';
            }
            first = false;
            os += mParentDevice->getButtonName((*iter).first);
        }
        os += "]";
        return os;
    }
    virtual unsigned int getNumAxes() const {
        return 1;
    }
    virtual int getNumButtons() const {
        return 0;
    }
    virtual std::string getButtonName(unsigned int button) const {
        return std::string();
    }

};

}
}

#endif