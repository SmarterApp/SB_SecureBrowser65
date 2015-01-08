/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
* Version: NPL 1.1/GPL 2.0/LGPL 2.1
*
* The contents of this file are subject to the Netscape Public License
* Version 1.1 (the "License"); you may not use this file except in
* compliance with the License. You may obtain a copy of the License at
* http://www.mozilla.org/NPL/
*
* Software distributed under the License is distributed on an "AS IS" basis,
* WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
* for the specific language governing rights and limitations under the
* License.
*
* The Original Code is mozilla.org code.
*
* The Initial Developer of the Original Code is 
* Netscape Communications Corporation.
* Portions created by the Initial Developer are Copyright (C) 1998
* the Initial Developer. All Rights Reserved.
*
* Contributor(s):
*
* Alternatively, the contents of this file may be used under the terms of
* either the GNU General Public License Version 2 or later (the "GPL"), or 
* the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
* in which case the provisions of the GPL or the LGPL are applicable instead
* of those above. If you wish to allow use of your version of this file only
* under the terms of either the GPL or the LGPL, and not to allow others to
* use your version of this file under the terms of the NPL, indicate your
* decision by deleting the provisions above and replace them with the notice
* and other provisions required by the GPL or the LGPL. If you do not delete
* the provisions above, a recipient may use your version of this file under
* the terms of any one of the NPL, the GPL or the LGPL.
*
* ***** END LICENSE BLOCK ***** */

//////////////////////////////////////////////////
//
// CPlugin class implementation
//
#ifdef XP_WIN
#include <stdio.h>
#include <windows.h>
#include <windowsx.h>
#endif

#ifdef XP_MAC
#include <TextEdit.h>
#endif

#ifdef XP_UNIX
#include <string.h>
#endif

#include <queue>
#include "plugin.h"
#include "npfunctions.h"

#include "audio.h"
#include "json_spirit_writer.h"
#include "json_spirit_reader.h"

static NPIdentifier sInitializeId;
static NPIdentifier sGetStatusCapId;
static NPIdentifier sGetCapabilitiesId;
static NPIdentifier sStartCaptureId;
static NPIdentifier sStopCaptureId;
static NPIdentifier sPlayId;
static NPIdentifier sStopPlayId;
static NPIdentifier sPausePlayId;
static NPIdentifier sResumePlayId;
static NPIdentifier sCleanId;
static NPIdentifier sDocument_id;
static NPIdentifier sBody_id;
static NPIdentifier sCreateElement_id;
static NPIdentifier sCreateTextNode_id;
static NPIdentifier sAppendChild_id;
static NPIdentifier sPluginType_id;
static NPObject *sWindowObj;

// Helper class that can be used to map calls to the NPObject hooks
// into virtual methods on instances of classes that derive from this
// class.
class ScriptablePluginObjectBase : public NPObject
{
public:
  ScriptablePluginObjectBase(NPP npp)
    : mNpp(npp)
  {
  }

  virtual ~ScriptablePluginObjectBase()
  {
  }

  // Virtual NPObject hooks called through this base class.
  virtual void Invalidate();
  virtual bool HasMethod(NPIdentifier name);
  virtual bool Invoke(NPIdentifier name, const NPVariant *args, uint32_t argCount, NPVariant *result);
  virtual bool InvokeDefault(const NPVariant *args, uint32_t argCount, NPVariant *result);
  virtual bool HasProperty(NPIdentifier name);
  virtual bool GetProperty(NPIdentifier name, NPVariant *result);
  virtual bool SetProperty(NPIdentifier name, const NPVariant *value);
  virtual bool RemoveProperty(NPIdentifier name);
  virtual bool Enumerate(NPIdentifier **identifier, uint32_t *count);
  virtual bool Construct(const NPVariant *args, uint32_t argCount, NPVariant *result);

public:
  static void _Deallocate(NPObject *npobj);
  static void _Invalidate(NPObject *npobj);
  static bool _HasMethod(NPObject *npobj, NPIdentifier name);
  static bool _Invoke(NPObject *npobj, NPIdentifier name, const NPVariant *args, uint32_t argCount, NPVariant *result);
  static bool _InvokeDefault(NPObject *npobj, const NPVariant *args, uint32_t argCount, NPVariant *result);
  static bool _HasProperty(NPObject * npobj, NPIdentifier name);
  static bool _GetProperty(NPObject *npobj, NPIdentifier name, NPVariant *result);
  static bool _SetProperty(NPObject *npobj, NPIdentifier name, const NPVariant *value);
  static bool _RemoveProperty(NPObject *npobj, NPIdentifier name);
  static bool _Enumerate(NPObject *npobj, NPIdentifier **identifier, uint32_t *count);
  static bool _Construct(NPObject *npobj, const NPVariant *args, uint32_t argCount, NPVariant *result);

protected:
  NPP mNpp;
};

#define DECLARE_NPOBJECT_CLASS_WITH_BASE(_class, ctor)                        \
  static NPClass s##_class##_NPClass = {                                      \
  NP_CLASS_STRUCT_VERSION_CTOR,                                               \
  ctor,                                                                       \
  ScriptablePluginObjectBase::_Deallocate,                                    \
  ScriptablePluginObjectBase::_Invalidate,                                    \
  ScriptablePluginObjectBase::_HasMethod,                                     \
  ScriptablePluginObjectBase::_Invoke,                                        \
  ScriptablePluginObjectBase::_InvokeDefault,                                 \
  ScriptablePluginObjectBase::_HasProperty,                                   \
  ScriptablePluginObjectBase::_GetProperty,                                   \
  ScriptablePluginObjectBase::_SetProperty,                                   \
  ScriptablePluginObjectBase::_RemoveProperty,                                \
  ScriptablePluginObjectBase::_Enumerate,                                     \
  ScriptablePluginObjectBase::_Construct                                      \
}

#define GET_NPOBJECT_CLASS(_class) &s##_class##_NPClass

void ScriptablePluginObjectBase::Invalidate()
{
}

bool ScriptablePluginObjectBase::HasMethod(NPIdentifier name)
{
  return false;
}

bool ScriptablePluginObjectBase::Invoke(NPIdentifier name, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
  return false;
}

bool ScriptablePluginObjectBase::InvokeDefault(const NPVariant *args, uint32_t argCount, NPVariant *result)
{
  return false;
}

bool ScriptablePluginObjectBase::HasProperty(NPIdentifier name)
{
  return false;
}

bool ScriptablePluginObjectBase::GetProperty(NPIdentifier name, NPVariant *result)
{
  return false;
}

bool ScriptablePluginObjectBase::SetProperty(NPIdentifier name, const NPVariant *value)
{
  return false;
}

bool ScriptablePluginObjectBase::RemoveProperty(NPIdentifier name)
{
  return false;
}

bool ScriptablePluginObjectBase::Enumerate(NPIdentifier **identifier, uint32_t *count)
{
  return false;
}

bool ScriptablePluginObjectBase::Construct(const NPVariant *args, uint32_t argCount, NPVariant *result)
{
  return false;
}

// static
void ScriptablePluginObjectBase::_Deallocate(NPObject *npobj)
{
  // Call the virtual destructor.
  delete (ScriptablePluginObjectBase *)npobj;
}

// static
void ScriptablePluginObjectBase::_Invalidate(NPObject *npobj)
{
  ((ScriptablePluginObjectBase *)npobj)->Invalidate();
}

// static
bool ScriptablePluginObjectBase::_HasMethod(NPObject *npobj, NPIdentifier name)
{
  return ((ScriptablePluginObjectBase *)npobj)->HasMethod(name);
}

// static
bool ScriptablePluginObjectBase::_Invoke(NPObject *npobj, NPIdentifier name, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
  return ((ScriptablePluginObjectBase *)npobj)->Invoke(name, args, argCount,
    result);
}

// static
bool ScriptablePluginObjectBase::_InvokeDefault(NPObject *npobj, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
  return ((ScriptablePluginObjectBase *)npobj)->InvokeDefault(args, argCount,
    result);
}

// static
bool ScriptablePluginObjectBase::_HasProperty(NPObject * npobj, NPIdentifier name)
{
  return ((ScriptablePluginObjectBase *)npobj)->HasProperty(name);
}

// static
bool ScriptablePluginObjectBase::_GetProperty(NPObject *npobj, NPIdentifier name, NPVariant *result)
{
  return ((ScriptablePluginObjectBase *)npobj)->GetProperty(name, result);
}

// static
bool ScriptablePluginObjectBase::_SetProperty(NPObject *npobj, NPIdentifier name, const NPVariant *value)
{
  return ((ScriptablePluginObjectBase *)npobj)->SetProperty(name, value);
}

// static
bool ScriptablePluginObjectBase::_RemoveProperty(NPObject *npobj, NPIdentifier name)
{
  return ((ScriptablePluginObjectBase *)npobj)->RemoveProperty(name);
}

// static
bool ScriptablePluginObjectBase::_Enumerate(NPObject *npobj, NPIdentifier **identifier, uint32_t *count)
{
  return ((ScriptablePluginObjectBase *)npobj)->Enumerate(identifier, count);
}

// static
bool ScriptablePluginObjectBase::_Construct(NPObject *npobj, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
  return ((ScriptablePluginObjectBase *)npobj)->Construct(args, argCount,
    result);
}


class ConstructablePluginObject : public ScriptablePluginObjectBase
{
public:
  ConstructablePluginObject(NPP npp) : ScriptablePluginObjectBase(npp)
  {
  }

  virtual bool Construct(const NPVariant *args, uint32_t argCount, NPVariant *result);
};

static NPObject * AllocateConstructablePluginObject(NPP npp, NPClass *aClass)
{
  return new ConstructablePluginObject(npp);
}

DECLARE_NPOBJECT_CLASS_WITH_BASE(ConstructablePluginObject, AllocateConstructablePluginObject);

bool
  ConstructablePluginObject::Construct(const NPVariant *args, uint32_t argCount, NPVariant *result)
{
  printf("Creating new ConstructablePluginObject!\n");

  NPObject *myobj =
    NPN_CreateObject(mNpp, GET_NPOBJECT_CLASS(ConstructablePluginObject));
  if (!myobj)
    return false;

  OBJECT_TO_NPVARIANT(myobj, *result);

  return true;
}

class ScriptablePluginObject : public ScriptablePluginObjectBase
{
  typedef struct
  {
    audio::Events audEvent;
    std::string data;
  } evtInfo;

public:
  ScriptablePluginObject(NPP npp) : ScriptablePluginObjectBase(npp)
  {
    initialized = false;
  }

  ~ScriptablePluginObject()
  {
    stopWatch = true;
    mNpp = 0;
  }

  virtual bool HasMethod(NPIdentifier name);
  virtual bool HasProperty(NPIdentifier name);
  virtual bool GetProperty(NPIdentifier name, NPVariant *result);
  virtual bool Invoke(NPIdentifier name, const NPVariant *args, uint32_t argCount, NPVariant *result);
  virtual bool InvokeDefault(const NPVariant *args, uint32_t argCount, NPVariant *result);

  bool addListener(std::string name, NPVariant obj);
  bool fireEvent(std::string name, std::string opts); 
  void writeToConsole(char* message);
  static void initThread(void*);

  bool initialize(const NPVariant *args, uint32_t argCount, NPVariant *result);
  bool clean(const NPVariant *args, uint32_t argCount, NPVariant *result);
  bool getStatus(const NPVariant *args, uint32_t argCount, NPVariant *result);
  bool getCapabilities(const NPVariant *args, uint32_t argCount, NPVariant *result);
  bool startCapture(const NPVariant *args, uint32_t argCount, NPVariant *result);
  bool stopCapture(const NPVariant *args, uint32_t argCount, NPVariant *result);
  bool play(const NPVariant *args, uint32_t argCount, NPVariant *result);
  bool stopPlay(const NPVariant *args, uint32_t argCount, NPVariant *result);
  bool pausePlay(const NPVariant *args, uint32_t argCount, NPVariant *result);
  bool resumePlay(const NPVariant *args, uint32_t argCount, NPVariant *result);
  audio audObj;
  NPObject *captureCallback;

  bool initialized;
  static void watchEvents(void *);
  static void eventCallback(audio::Events evt, std::string data);
  static std::queue<evtInfo> evtCol;
  static bool stopWatch;

};

static NPObject * AllocateScriptablePluginObject(NPP npp, NPClass *aClass)
{
  return new ScriptablePluginObject(npp);
}

DECLARE_NPOBJECT_CLASS_WITH_BASE(ScriptablePluginObject, AllocateScriptablePluginObject);

bool ScriptablePluginObject::HasMethod(NPIdentifier name)
{
  bool bRetval = false;

  if (name == sInitializeId)
  {
    bRetval = true;
  }
  else if (name == sGetStatusCapId)
  {
    bRetval = true;
  }
  else if (name == sGetCapabilitiesId)
  {
    bRetval = true;
  }
  else if (name == sStartCaptureId)
  {
    bRetval = true;
  }
  else if (name == sStopCaptureId)
  {
    bRetval = true;
  }
  else if (name == sPlayId)
  {
    bRetval = true;
  }
  else if (name == sStopPlayId)
  {
    bRetval = true;
  }
  else if (name == sPausePlayId)
  {
    bRetval = true;
  }
  else if (name == sResumePlayId)
  {
    bRetval = true;
  }
  else if (name == sCleanId)
  {
    bRetval = true;
  }

  return bRetval;
}

bool ScriptablePluginObject::HasProperty(NPIdentifier name)
{
  return (name == sPluginType_id);
}

bool ScriptablePluginObject::GetProperty(NPIdentifier name, NPVariant *result)
{
  VOID_TO_NPVARIANT(*result);

  //if (name == sBar_id) {
  //  static int a = 16;

  //  INT32_TO_NPVARIANT(a, *result);

  //  a += 5;

  //  return true;
  //}

  if (name == sPluginType_id) {
    NPObject *myobj =
      NPN_CreateObject(mNpp, GET_NPOBJECT_CLASS(ConstructablePluginObject));
    if (!myobj) {
      return false;
    }

    OBJECT_TO_NPVARIANT(myobj, *result);

    return true;
  }

  return true;
}

void ScriptablePluginObject::initThread(void* ptr)
{
  ScriptablePluginObject* inst = (ScriptablePluginObject*)ptr;
  if (inst->audObj.initialize(eventCallback))
  {
    //send event "READY"
    //inst->fireEvent("READY", "ready json dat");
  }
  else
  {
    //send event "ERROR"
    //inst->fireEvent("ERROR", "error json dat");
  }
}

bool ScriptablePluginObject::clean(const NPVariant *args, uint32_t argCount, NPVariant *result)
{
  if (initialized)
  {
    stopWatch = true;
    Pa_Sleep(300);
  }
  if (!evtCol.empty())
  {
    std::queue<ScriptablePluginObject::evtInfo> empty;
    std::swap( evtCol, empty );
  }
  initialized = false;
  audObj.clean();
  return PR_TRUE;
}

bool ScriptablePluginObject::initialize(const NPVariant *args, uint32_t argCount, NPVariant *result)
{
  if (initialized)
  {
    stopWatch = true;
    Pa_Sleep(300);
  }
  if (!evtCol.empty())
  {
    std::queue<ScriptablePluginObject::evtInfo> empty;
    std::swap( evtCol, empty );
  }
  if (argCount > 0)
  {
    //setup listener for events
    if (args[0].type == NPVariantType_Object)
    {
      captureCallback = NPVARIANT_TO_OBJECT(args[0]);
    }
  }

        stopWatch = false;
  NPN_PluginThreadAsyncCall(mNpp, initThread, this);
  NPN_PluginThreadAsyncCall(mNpp, watchEvents, this);
  initialized = true;
  return PR_TRUE;
}

bool ScriptablePluginObject::getStatus(const NPVariant *args, uint32_t argCount, NPVariant *result)
{
  char* out = (char *)NPN_MemAlloc(255);
  audObj.getStatus(out);
  STRINGZ_TO_NPVARIANT(out, *result);
  return PR_TRUE;
}

bool ScriptablePluginObject::getCapabilities(const NPVariant *args, uint32_t argCount, NPVariant *result)
{
  json_spirit::Object devices;
  if (audObj.getCapabilities(devices))
  {
    std::string output;
    output = json_spirit::write(devices, json_spirit::pretty_print);
    char *out = (char*)NPN_MemAlloc(output.length()+1);
    strcpy(out, output.c_str());
    STRINGZ_TO_NPVARIANT(out, *result);
    return true;
  }
  else
    return false;
}

///////////////////////////////////////////////////////////////////
/////////// CALL A FUNCTION:
// Get window object.
//NPObject* window = NULL;
//NPN_GetValue(mNpp, NPNVWindowNPObject, &window);

//NPIdentifier id = NPN_GetStringIdentifier(name.c_str());
//NPVariant type;
//NPVariant voidResponse;
//NPN_Invoke(mNpp, window, id, NULL, 0, /* args, sizeof(args) / sizeof(args[0]), */ &voidResponse);

//// Cleanup all allocated objects
//NPN_ReleaseObject(window);
//NPN_ReleaseVariantValue(&voidResponse);
///////////////////////////////////////////////////////////////////

bool ScriptablePluginObject::addListener(std::string name, NPVariant obj)
{

  ///* Get window object */
  //NPObject* window = NULL;
  //NPN_GetValue(mNpp, NPNVWindowNPObject, &window);

  ///* Get document object */
  //NPVariant documentVar;
  //NPIdentifier id = NPN_GetStringIdentifier("document");
  //NPN_GetProperty(mNpp, window, id, &documentVar);
  //NPObject* document = NPVARIANT_TO_OBJECT(documentVar);

  ///* Get event object. */
  //NPVariant resultVar;
  //id = NPN_GetStringIdentifier("addEventListener");

  //{ /* Note that I just like to use the words |args| and |type| again. */
  //NPVariant type;
  //STRINGZ_TO_NPVARIANT(name.c_str(), type);
  //NPVariant args[] = {type, obj};
  //NPN_Invoke(mNpp, document, id, args, sizeof(args)/sizeof(args[0]),
  //      &resultVar);
  //}
  
  //NPObject* evt = NPVARIANT_TO_OBJECT(resultVar);
  return true;
}

bool ScriptablePluginObject::fireEvent(std::string name, std::string eventParams)
{
  if (!mNpp) return false;

  //writeToConsole((char*)name.c_str());

  /* Get window object */
  NPObject* window = NULL;
  NPN_GetValue(mNpp, NPNVWindowNPObject, &window);

  /* Get document object */
  NPVariant documentVar;
  NPIdentifier id = NPN_GetStringIdentifier("document");
  NPN_GetProperty(mNpp, window, id, &documentVar);
  NPObject* document = NPVARIANT_TO_OBJECT(documentVar);

  /* Get event object. */
  NPVariant evtVar;
  id = NPN_GetStringIdentifier("createEvent");

  { /* Note that I just like to use the words |args| and |type| again. */
  NPVariant type;
  STRINGZ_TO_NPVARIANT("CustomEvent", type);
  NPVariant args[] = {type};
  NPN_Invoke(mNpp, document, id, args, sizeof(args)/sizeof(args[0]), &evtVar);
  }

  NPObject* evt = NPVARIANT_TO_OBJECT(evtVar);

  NPVariant dummyResult;
  id = NPN_GetStringIdentifier("initCustomEvent");
  {
  NPVariant type, canBubble, cancelable, opts;
  /* Initialize args: */
  STRINGZ_TO_NPVARIANT(name.c_str(), type);
  BOOLEAN_TO_NPVARIANT(true, canBubble);     
  BOOLEAN_TO_NPVARIANT(true, cancelable); 
  STRINGZ_TO_NPVARIANT(eventParams.c_str(), opts);
  NPVariant args[] = {
    type, canBubble, cancelable, opts
  };

  NPN_Invoke(mNpp, evt, id, args, sizeof(args)/sizeof(args[0]), &dummyResult);

  }

  /* Get recorder element */
  //NPVariant recorderVar;
  //id = NPN_GetStringIdentifier("getElementById");
  //{
  //NPVariant elementId;
  //STRINGZ_TO_NPVARIANT("recorder", elementId);
  //NPVariant args[] = {elementId};
  //NPN_Invoke(mNpp, document, id, args, sizeof(args)/sizeof(args[0]),
  //      &recorderVar);
  //}
  //NPObject* recorder = NPVARIANT_TO_OBJECT(recorderVar);

  /* Fire |evt| */
  NPVariant notCanceled; /* We can't receive "canceled" directly! */
  id = NPN_GetStringIdentifier("dispatchEvent");
  {
  NPVariant args[] = {evtVar};
  NPN_Invoke(mNpp, document, id, args, sizeof(args)/sizeof(args[0]),
        &notCanceled);
  }

  bool canceled = NPVARIANT_IS_BOOLEAN(notCanceled) &&
          !(NPVARIANT_TO_BOOLEAN(notCanceled));
  if (canceled) {
    // A handler called preventDefault
    //printf("canceled\n");
  }
  else {
    // None of the handlers called preventDefault
    //printf("not canceled\n");
  }

  /* Release all allocated objects, otherwise, reference count/memory leaks. */
  NPN_ReleaseObject(window);
  NPN_ReleaseVariantValue(&documentVar);
  NPN_ReleaseVariantValue(&evtVar);
  NPN_ReleaseVariantValue(&dummyResult); /* fake */
  //NPN_ReleaseVariantValue(&recorderVar);
  NPN_ReleaseVariantValue(&notCanceled); /* fake*/
  return true;
}

bool ScriptablePluginObject::startCapture(const NPVariant *args, uint32_t argCount, NPVariant *result)
{
  if (argCount == 2)
  {
    if (args[1].type == NPVariantType_Object)
    {
      captureCallback = NPVARIANT_TO_OBJECT(args[1]);
    }

    if (args[0].type == NPVariantType_String)
    {
      int deviceId=0;
      int sampleRate=0;
      int channelCount=0;
      int sampleSize=0;
      std::string encodingFormat;
      bool quality=false;
      std::string reportFreqType;
      int reportInterval=0;
      std::string limitType;
      int limitSize=0;

      std::string optstr;
      optstr.reserve(args[0].value.stringValue.UTF8Length);
      optstr.append(args[0].value.stringValue.UTF8Characters);
      json_spirit::Value value;
      json_spirit::read( optstr, value );
      const json_spirit::Object& options = value.get_obj();
      for( json_spirit::Object::size_type i = 0; i != options.size(); ++i )
      {
        const json_spirit::Pair& pair = options[i];

        const std::string& name  = pair.name_;
        const json_spirit::Value&  value = pair.value_;

        if ( name == "captureDevice" )
        {
          deviceId = value.get_int();
        }
        else if ( name == "sampleRate" )
        {
          if (value.type() == json_spirit::int_type)
          {
            sampleRate = value.get_int();
          }
          else if (value.type() == json_spirit::str_type)
          {
            sampleRate = strtol( value.get_str().c_str(), NULL, 10);
          }
        }
        else if ( name == "channelCount" )
        {
          if (value.type() == json_spirit::int_type)
          {
            channelCount = value.get_int();
          }
          else if (value.type() == json_spirit::str_type)
          {
            channelCount = strtol( value.get_str().c_str(), NULL, 10);
          }
        }
        else if ( name == "sampleSize" )
        {
          if (value.type() == json_spirit::int_type)
          {
            sampleSize = value.get_int();
          }
          else if (value.type() == json_spirit::str_type)
          {
            sampleSize = strtol( value.get_str().c_str(), NULL, 10);
          }
        }
        else if ( name == "encodingFormat" )
        {
          encodingFormat = value.get_str();
        }
        else if ( name == "qualityIndicator" )
        {
          if (value.type() == json_spirit::bool_type)
          {
            quality = value.get_bool();
          }
          else if (value.type() == json_spirit::str_type)
          {
            quality = value.get_str().compare("true") == 0;
          }
        }
        else if (name == "progressFrequency")
        {
          if (value.type() == json_spirit::obj_type)
          {
            const json_spirit::Object& freq = value.get_obj();
            for( json_spirit::Object::size_type j = 0; j != freq.size(); ++j )
            {
              const json_spirit::Pair& pr = freq[j];

              const std::string& name  = pr.name_;
              const json_spirit::Value&  val = pr.value_;

              if ( name == "type" )
              {
                reportFreqType = val.get_str();
              }
              else if ( name == "interval" )
              {
                reportInterval = val.get_int();
                if (reportFreqType.compare("size") == 0)
                {
                  reportInterval = reportInterval * 1024;  //convert interval to bytes
                }
              }
            }
          }
        }
        //"captureLimit": { "type": "size", "limit": 250 } 
        else if (name == "captureLimit")
        {
          if (value.type() == json_spirit::obj_type)
          {
            const json_spirit::Object& limit = value.get_obj();
            for( json_spirit::Object::size_type j = 0; j != limit.size(); ++j )
            {
              const json_spirit::Pair& pr = limit[j];

              const std::string& name  = pr.name_;
              const json_spirit::Value&  val = pr.value_;

              if ( name == "type" )
              {
                limitType = val.get_str();
              }
              else if ( name == "limit" )
              {
                limitSize = val.get_int();
                if (limitType.compare("size") == 0)
                {
                  limitSize = limitSize * 1024;
                }
              }
            }
          }
        }
        else
        {
          // assert( false );
        }
      }

      if (!audObj.startCapture(deviceId, sampleRate, channelCount, sampleSize, encodingFormat, quality, reportFreqType, reportInterval, limitType, limitSize, eventCallback))
      {
        return PR_FALSE;
      }
    }
  }
  //char* src = "recording...";
  //char* out = (char *)NPN_MemAlloc(strlen(src) + 1);
  //strcpy(out, src);
  //STRINGZ_TO_NPVARIANT(out, *result);
  return PR_TRUE;
}

bool ScriptablePluginObject::stopCapture(const NPVariant *args, uint32_t argCount, NPVariant *result)
{
  audObj.stopCapture();
  //char* src = "stopped";
  //char* out = (char *)NPN_MemAlloc(strlen(src) + 1);
  //strcpy(out, src);
  //STRINGZ_TO_NPVARIANT(out, *result);
  return PR_TRUE;
}

bool ScriptablePluginObject::play(const NPVariant *args, uint32_t argCount, NPVariant *result)
{
  std::string audioData;
  
  if (argCount > 0)
  {
    if (args[0].type == NPVariantType_String)
    {

      std::string optstr;
      optstr.reserve(args[0].value.stringValue.UTF8Length);
      optstr.append(args[0].value.stringValue.UTF8Characters);
      json_spirit::Value value;
      json_spirit::read( optstr, value );
      const json_spirit::Object& options = value.get_obj();
      for( json_spirit::Object::size_type i = 0; i != options.size(); ++i )
      {
        const json_spirit::Pair& pair = options[i];

        const std::string& name  = pair.name_;
        const json_spirit::Value&  value = pair.value_;

        if ( name == "data" )
        {
          audioData = value.get_str();
        }
        else
        {
          // assert(false);
        }
      }
    }
  audObj.play(audioData, eventCallback);
  }
  return PR_TRUE;
}

bool ScriptablePluginObject::stopPlay(const NPVariant *args, uint32_t argCount, NPVariant *result)
{
  audObj.stopPlay();
  return PR_TRUE;
}

bool ScriptablePluginObject::pausePlay(const NPVariant *args, uint32_t argCount, NPVariant *result)
{
  audObj.pausePlay();
  return PR_TRUE;
}

bool ScriptablePluginObject::resumePlay(const NPVariant *args, uint32_t argCount, NPVariant *result)
{
  audObj.resumePlay();
  return PR_TRUE;
}

void ScriptablePluginObject::eventCallback(audio::Events e, std::string data)
{
  evtInfo info = {e, data};
  evtCol.push(info);
}

void ScriptablePluginObject::watchEvents(void *ptr)
{
  ScriptablePluginObject* inst = (ScriptablePluginObject*)ptr;

  if (!inst) return;

  if (!evtCol.empty())
  {
    evtInfo info = evtCol.front();
    evtCol.pop();
    switch(info.audEvent)
    {
      case audio::INITIALIZING:
        inst->fireEvent("INITIALIZING", info.data);
        break;
      case audio::READY:
        inst->fireEvent("READY", info.data);
        break;
      case audio::AUDIOERROR:
        inst->fireEvent("AUDIOERROR", info.data);
        break;
      case audio::START:
        inst->fireEvent("START", info.data);
        break;
      case audio::INPROGRESS:
        inst->fireEvent("INPROGRESS", info.data);
        break;
      case audio::END:
        inst->fireEvent("END", info.data);
        break;
      case audio::PLAYSTART:
        inst->fireEvent("PLAYSTART", info.data);
        break;
      case audio::PLAYSTOP:
        inst->fireEvent("PLAYSTOP", info.data);
        break;
      case audio::PLAYPAUSED:
        inst->fireEvent("PLAYPAUSED", info.data);
        break;
      case audio::PLAYRESUMED:
        inst->fireEvent("PLAYRESUMED", info.data);
        break;
      default:
        break;
    }
  }

  if (!stopWatch)
  {
#if defined(XP_UNIX)
#ifndef XP_MACOSX
    // This causes crash when we leave page while recording --pete
    Pa_Sleep(200);
#endif
#endif

    NPN_PluginThreadAsyncCall(inst->mNpp, watchEvents, inst);
  }
}

void ScriptablePluginObject::writeToConsole(char* message)
{
  // The message we want to send.
  //char* message = "Hello from C++";
  
  // Get window object.
  NPObject* window = NULL;
  NPN_GetValue(mNpp, NPNVWindowNPObject, &window);

  // Get console object.
  NPVariant consoleVar;
  NPIdentifier id = NPN_GetStringIdentifier("console");
  NPN_GetProperty(mNpp, window, id, &consoleVar);
  NPObject* console = NPVARIANT_TO_OBJECT(consoleVar);

  // Get the debug object.
  id = NPN_GetStringIdentifier("debug");

  // Invoke the call with the message!
  NPVariant type;
  STRINGZ_TO_NPVARIANT(message, type);
  NPVariant args[] = { type };
  NPVariant voidResponse;
  NPN_Invoke(mNpp, console, id, args,
             sizeof(args) / sizeof(args[0]),
             &voidResponse);

  // Cleanup all allocated objects, otherwise, reference count and
  // memory leaks will happen.
  NPN_ReleaseObject(window);
  NPN_ReleaseVariantValue(&consoleVar);
  NPN_ReleaseVariantValue(&voidResponse);
}

bool ScriptablePluginObject::Invoke(NPIdentifier name, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
  bool bRetval = PR_FALSE;

  if (name == sInitializeId) 
  {
    bRetval = initialize(args, argCount, result);
  }
  else if (name == sGetStatusCapId)
  {
    bRetval = getStatus(args, argCount, result);
  }
  else if (name == sGetCapabilitiesId)
  {
    if (initialized)
    bRetval = getCapabilities(args, argCount, result);
  }
  else if (name == sStartCaptureId)
  {
    if (initialized)
    bRetval = startCapture(args, argCount, result);
  }
  else if (name == sStopCaptureId)
  {
    if (initialized)
    bRetval = stopCapture(args, argCount, result);
  }
  else if (name == sPlayId)
  {
    if (initialized)
    bRetval = play(args, argCount, result);
  }
  else if (name == sStopPlayId)
  {
    if (initialized)
    bRetval = stopPlay(args, argCount, result);
  }
  else if (name == sPausePlayId)
  {
    if (initialized)
    bRetval = pausePlay(args, argCount, result);
  }
  else if (name == sResumePlayId)
  {
    if (initialized)
    bRetval = resumePlay(args, argCount, result);
  }
  else if (name == sCleanId)
  {
    bRetval = clean(args, argCount, result);
  }

  return bRetval;
}

bool ScriptablePluginObject::InvokeDefault(const NPVariant *args, uint32_t argCount, NPVariant *result)
{
  printf ("ScriptablePluginObject default method called!\n");

  STRINGZ_TO_NPVARIANT(strdup("default method return val"), *result);

  return PR_TRUE;
}

std::queue<ScriptablePluginObject::evtInfo> ScriptablePluginObject::evtCol;

bool ScriptablePluginObject::stopWatch = false;

CPlugin::CPlugin(NPP pNPInstance) :
  m_pNPInstance(pNPInstance),
  m_pNPStream(NULL),
  m_bInitialized(PR_FALSE),
  m_pScriptableObject(NULL)
{
#ifdef XP_WIN
  m_hWnd = NULL;
#endif

  NPN_GetValue(m_pNPInstance, NPNVWindowNPObject, &sWindowObj);

  // NPIdentifier n = NPN_GetStringIdentifier("foof");

  sInitializeId = NPN_GetStringIdentifier("initialize");
  sGetStatusCapId = NPN_GetStringIdentifier("getStatus");
  sGetCapabilitiesId = NPN_GetStringIdentifier("getCapabilities");
  sStartCaptureId = NPN_GetStringIdentifier("startCapture");
  sStopCaptureId = NPN_GetStringIdentifier("stopCapture");
  sPlayId = NPN_GetStringIdentifier("play");
  sStopPlayId = NPN_GetStringIdentifier("stopPlay");
  sPausePlayId = NPN_GetStringIdentifier("pausePlay");
  sResumePlayId = NPN_GetStringIdentifier("resumePlay");
  sCleanId = NPN_GetStringIdentifier("clean");

  sDocument_id = NPN_GetStringIdentifier("document");
  sBody_id = NPN_GetStringIdentifier("body");
  sCreateElement_id = NPN_GetStringIdentifier("createElement");
  sCreateTextNode_id = NPN_GetStringIdentifier("createTextNode");
  sAppendChild_id = NPN_GetStringIdentifier("appendChild");
  sPluginType_id = NPN_GetStringIdentifier("PluginType");

  /********** THIS IS UNNECESSARY TEST CODE --pete
  NPVariant v;
  INT32_TO_NPVARIANT(46, v);

  NPN_SetProperty(m_pNPInstance, sWindowObj, n, &v);

  NPVariant rval;
  NPN_GetProperty(m_pNPInstance, sWindowObj, n, &rval);

  if (NPVARIANT_IS_INT32(rval)) {
    printf("rval = %d\n", NPVARIANT_TO_INT32(rval));
  }

  n = NPN_GetStringIdentifier("document");

  if (!NPN_IdentifierIsString(n)) {
    NPString str;
    str.UTF8Characters = "alert('NPN_IdentifierIsString() test failed!');";
    str.UTF8Length = strlen(str.UTF8Characters);

    NPN_Evaluate(m_pNPInstance, sWindowObj, &str, NULL);
  }

  NPObject *doc;

  NPN_GetProperty(m_pNPInstance, sWindowObj, n, &rval);

  if (NPVARIANT_IS_OBJECT(rval) && (doc = NPVARIANT_TO_OBJECT(rval))) {
    n = NPN_GetStringIdentifier("title");

    NPN_GetProperty(m_pNPInstance, doc, n, &rval);

    if (NPVARIANT_IS_STRING(rval)) {
      printf ("title = %s\n", NPVARIANT_TO_STRING(rval).UTF8Characters);

      NPN_ReleaseVariantValue(&rval);
    }

    n = NPN_GetStringIdentifier("plugindoc");

    OBJECT_TO_NPVARIANT(doc, v);
    NPN_SetProperty(m_pNPInstance, sWindowObj, n, &v);

    //NPString str;
    //str.UTF8Characters = "document.getElementById('result').innerHTML += '<p>' + 'NPN_Evaluate() test, document = ' + this + '</p>';";
    //str.UTF8Length = strlen(str.UTF8Characters);

    //NPN_Evaluate(m_pNPInstance, doc, &str, NULL);

    NPN_ReleaseObject(doc);
  }

  NPVariant arg;
  OBJECT_TO_NPVARIANT(sWindowObj, arg);

  if (NPVARIANT_IS_INT32(rval) && NPVARIANT_TO_INT32(rval) == 4) {
    printf ("Default function call SUCCEEDED!\n");
  } else {
    printf ("Default function call FAILED!\n");
  }

  NPN_ReleaseVariantValue(&rval);
  **********/


#if 0
  n = NPN_GetStringIdentifier("prompt");

  NPVariant vars[3];
  STRINGZ_TO_NPVARIANT("foo", vars[0]);
  STRINGZ_TO_NPVARIANT("bar", vars[1]);
  STRINGZ_TO_NPVARIANT("foof", vars[2]);

  NPN_Invoke(sWindowObj, n, vars, 3, &rval);

  if (NPVARIANT_IS_STRING(rval)) {
    printf ("prompt returned '%s'\n", NPVARIANT_TO_STRING(rval).utf8characters);
  }

  NPN_ReleaseVariantValue(&rval);
#endif

  /********** THIS IS UNNECESSARY TEST CODE --pete
  NPObject *myobj =
    NPN_CreateObject(m_pNPInstance,
    GET_NPOBJECT_CLASS(ScriptablePluginObject));

  n = NPN_GetStringIdentifier("pluginobj");

  OBJECT_TO_NPVARIANT(myobj, v);
  NPN_SetProperty(m_pNPInstance, sWindowObj, n, &v);

  NPN_GetProperty(m_pNPInstance, sWindowObj, n, &rval);

  printf ("Object set/get test ");

  if (NPVARIANT_IS_OBJECT(rval) && NPVARIANT_TO_OBJECT(rval) == myobj) {
    printf ("succeeded!\n");
  } else {
    printf ("FAILED!\n");
  }

  NPN_ReleaseVariantValue(&rval);
  NPN_ReleaseObject(myobj);
  **********/

  const char *ua = NPN_UserAgent(m_pNPInstance);
  strcpy(m_String, "AIRRecorder ");
  strcat(m_String, ua);
}

CPlugin::~CPlugin()
{
  /********** THIS IS UNNECESSARY TEST CODE --pete
  NPObject* window = NULL;
  NPN_GetValue(m_pNPInstance, NPNVWindowNPObject, &window);

  NPObject *myobj = NPN_CreateObject(m_pNPInstance, GET_NPOBJECT_CLASS(ScriptablePluginObject));

  NPVariant voidResponse;
  NPN_Invoke(m_pNPInstance, window, NPN_GetStringIdentifier("clean"), NULL, 0, &voidResponse);

  NPN_ReleaseObject(myobj);
  NPN_ReleaseObject(window);
  NPN_ReleaseVariantValue(&voidResponse);
  **********/

  if (m_pScriptableObject)
    NPN_ReleaseObject(m_pScriptableObject);

  if (sWindowObj)
    NPN_ReleaseObject(sWindowObj);

  sInitializeId = 0;
  sGetStatusCapId = 0;
  sGetCapabilitiesId = 0;
  sStartCaptureId = 0;
  sStopCaptureId = 0;
  sPlayId = 0;
  sStopPlayId = 0;
  sPausePlayId = 0;
  sResumePlayId = 0;
  sCleanId = 0;
  sDocument_id = 0;
  sBody_id = 0;
  sCreateElement_id = 0;
  sCreateTextNode_id = 0;
  sAppendChild_id = 0;
  sPluginType_id = 0;

  sWindowObj = 0;
}

#ifdef XP_WIN
static LRESULT CALLBACK PluginWinProc(HWND, UINT, WPARAM, LPARAM);
static WNDPROC lpOldProc = NULL;
#endif

NPBool CPlugin::init(NPWindow* pNPWindow)
{
  if (pNPWindow == NULL)
    return PR_FALSE;

#ifdef XP_WIN
  m_hWnd = (HWND)pNPWindow->window;
  if (m_hWnd == NULL)
    return PR_FALSE;

  // subclass window so we can intercept window messages and
  // do our drawing to it
  lpOldProc = SubclassWindow(m_hWnd, (WNDPROC)PluginWinProc);

  // associate window with our CPlugin object so we can access 
  // it in the window procedure
  SetWindowLong(m_hWnd, GWL_USERDATA, (LONG)this);
#endif

  m_Window = pNPWindow;

  m_bInitialized = PR_TRUE;
  return PR_TRUE;
}

void CPlugin::shut()
{
#ifdef XP_WIN
  // subclass it back
  SubclassWindow(m_hWnd, lpOldProc);
  m_hWnd = NULL;
#endif

  m_bInitialized = PR_FALSE;
}

NPBool CPlugin::isInitialized()
{
  return m_bInitialized;
}

int16 CPlugin::handleEvent(void* event)
{
#ifdef XP_MAC
  NPEvent* ev = (NPEvent*)event;
  if (m_Window) {
    Rect box = { m_Window->y, m_Window->x,
      m_Window->y + m_Window->height, m_Window->x + m_Window->width };
    if (ev->what == updateEvt) {
      ::TETextBox(m_String, strlen(m_String), &box, teJustCenter);
    }
  }
#endif
  return 0;
}

// this will draw a version string in the plugin window
void CPlugin::showVersion()
{
  const char *ua = NPN_UserAgent(m_pNPInstance);
  strcpy(m_String, ua);

#ifdef XP_WIN
  InvalidateRect(m_hWnd, NULL, PR_TRUE);
  UpdateWindow(m_hWnd);
#endif

  if (m_Window) {
    NPRect r =
    {
      (uint16)m_Window->y, (uint16)m_Window->x,
      (uint16)(m_Window->y + m_Window->height),
      (uint16)(m_Window->x + m_Window->width)
    };

    NPN_InvalidateRect(m_pNPInstance, &r);
  }
}

// this will clean the plugin window
void CPlugin::clear()
{
  strcpy(m_String, "");

#ifdef XP_WIN
  InvalidateRect(m_hWnd, NULL, PR_TRUE);
  UpdateWindow(m_hWnd);
#endif
}

void CPlugin::getVersion(char* *aVersion)
{
  const char *ua = NPN_UserAgent(m_pNPInstance);
  char*& version = *aVersion;
  version = (char*)NPN_MemAlloc(1 + strlen(ua));
  if (version)
    strcpy(version, ua);
}

NPObject* CPlugin::GetScriptableObject()
{
  if (!m_pScriptableObject) {
    m_pScriptableObject =
      NPN_CreateObject(m_pNPInstance,
      GET_NPOBJECT_CLASS(ScriptablePluginObject));
  }

  if (m_pScriptableObject) {
    NPN_RetainObject(m_pScriptableObject);
  }

  return m_pScriptableObject;
}

#ifdef XP_WIN
static LRESULT CALLBACK PluginWinProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch (msg) {
  case WM_PAINT:
    {
      // draw a frame and display the string
#ifdef DEBUG
      PAINTSTRUCT ps;
      HDC hdc = BeginPaint(hWnd, &ps);
      RECT rc;
      GetClientRect(hWnd, &rc);
      FrameRect(hdc, &rc, GetStockBrush(BLACK_BRUSH));
      CPlugin * p = (CPlugin *)GetWindowLong(hWnd, GWL_USERDATA);
      if (p) {
        if (p->m_String[0] == 0) {
          strcpy("foo", p->m_String);
        }

        DrawText(hdc, p->m_String, strlen(p->m_String), &rc, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
      }

      EndPaint(hWnd, &ps);
#endif
    }
    break;
  default:
    break;
  }

  return DefWindowProc(hWnd, msg, wParam, lParam);
}
#endif
