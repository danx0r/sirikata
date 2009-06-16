/*  Sirikata
 *  main.cpp
 *
 *  Copyright (c) 2008, Daniel Reiter Horn
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

#include <oh/Platform.hpp>
#include <options/Options.hpp>
#include <util/PluginManager.hpp>
#include "DemoProxyManager.hpp"
#include <oh/SimulationFactory.hpp>
namespace Sirikata {
//InitializeOptions main_options("verbose",

}

int main ( int argc,const char**argv ) {
    using namespace Sirikata;
    PluginManager plugins;
    plugins.load (
#ifdef __APPLE__
#ifdef NDEBUG
        "libogregraphics.dylib"
#else
        "libogregraphics_d.dylib"
#endif
#else
#ifdef _WIN32
#ifdef NDEBUG
        "ogregraphics.dll"
#else
        "ogregraphics_d.dll"
#endif
#else
#ifdef NDEBUG
        "libogregraphics.so"
#else
        "libogregraphics_d.so"
#endif
#endif
#endif
    );
    plugins.load ("libbulletphysics_d.so");
    OptionSet::getOptions ( "" )->parse ( argc,argv );
    ProxyManager * pm=new DemoProxyManager;
    Provider<ProxyCreationListener*>*provider=pm;
    String graphicsCommandArguments;
    String graphicsPluginName ( "ogregraphics" );
    String physicsPluginName ( "bulletphysics" );
    SILOG(cppoh,error,"dbm: initializing graphics");
    TimeSteppedSimulation *graphicsSystem=
        SimulationFactory::getSingleton()
        .getConstructor ( graphicsPluginName ) ( provider,graphicsCommandArguments );
    SILOG(cppoh,error,"dbm: initializing physics");
    TimeSteppedSimulation *physicsSystem=
        SimulationFactory::getSingleton()
        .getConstructor ( physicsPluginName ) ( provider,graphicsCommandArguments );
    SILOG(cppoh,error,"dbm: checking physics");
    if (!physicsSystem) {
        SILOG(cppoh,error,"physicsSystem NULL!");
        int n=0;
        n /= 0;
    }
    else {
        SILOG(cppoh,error,"physicsSystem: " << std::hex << (unsigned long)physicsSystem);
    }
    SILOG(cppoh,error,"dbm: about to initialize");
    pm->initialize();
    if ( graphicsSystem ) {
        while ( graphicsSystem->tick() ) {
            SILOG ( cppoh,error,"just called ogre tick" ); ///dbm
            physicsSystem->tick();
            SILOG ( cppoh,error,"just called bullet tick" ); ///dbm
        }
    }
    else {
        SILOG ( cppoh,error,"Fatal Error: Unable to load OGRE Graphics plugin. The PATH environment variable is ignored, so make sure you have copied the DLLs from dependencies/ogre/bin/ into the current directory. Sorry about this!" );
    }
    pm->destroy();
    delete graphicsSystem;
    delete pm;
    plugins.gc();
    SimulationFactory::destroy();
    return 0;
}
