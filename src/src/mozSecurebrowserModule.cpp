/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Securebrowser code.
 *
 * The Initial Developer of the Original Code is
 * Securebrowser for Securebrowser Free Software Development.
 * Portions created by the Initial Developer are Copyright (C) 2009 
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsCRT.h"
#include "mozilla/ModuleUtils.h"
#include "nsIClassInfoImpl.h"

#ifdef XP_WIN
  #include "mozSecurebrowserWIN.h"
#endif

#ifdef XP_MACOSX
  #include "mozSecurebrowserOSX.h"
#endif

#ifdef XP_UNIX
  #include "mozSecurebrowserLinux.h"
#endif

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(mozSecurebrowser, Init)

// {E607BE99-1CF4-4FE1-BCDC-502C6F867ECB}
#define MOZ_SECUREBROWSER_CID \
{ 0xE607BE99, 0x1CF4, 0x4FE1, { 0xBC, 0xDC, 0x50, 0x2C, 0x6F, 0x86, 0x7E, 0xCB } }

/* 2D33DC5C-4A6A-4DD3-ABFA-892FAE678406 */
#define SECUREBROWSER_DOMCI_EXTENSION_CID   \
{ 0x2D33DC5C, 0x4A6A, 0x4DD3, {0xAB, 0xFA, 0x89, 0x2F, 0xAE, 0x67, 0x84, 0x06} }

#define SECUREBROWSER_DOMCI_EXTENSION_CONTRACTID \
"@mozilla.org/runtime-domci-extender;1"

NS_DEFINE_NAMED_CID(MOZ_SECUREBROWSER_CID);

static const mozilla::Module::CIDEntry kSecureBrowserCIDs[] = {
    { &kMOZ_SECUREBROWSER_CID, false, NULL, mozSecurebrowserConstructor },
    { NULL }
};

static const mozilla::Module::ContractIDEntry kSecureBrowserContracts[] = {
    { MOZ_SECUREBROWSER_CONTRACT_ID, &kMOZ_SECUREBROWSER_CID },
    { NULL }
};

static const mozilla::Module::CategoryEntry kSecureBrowserCategories[] = {
    { "Runtime", "securebrowser", MOZ_SECUREBROWSER_CONTRACT_ID },
    { NULL }
};

static const mozilla::Module kSecureBrowserModule = {
    mozilla::Module::kVersion,
    kSecureBrowserCIDs,
    kSecureBrowserContracts,
    kSecureBrowserCategories
};

NSMODULE_DEFN(mozSecurebrowserModule) = &kSecureBrowserModule;

/********
static NS_METHOD
RegisterRuntime(nsIComponentManager *aCompMgr,
                  nsIFile *aPath,
                  const char *registryLocation,
                  const char *componentType,
                  const nsModuleComponentInfo *info)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsICategoryManager> catman = do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
                                                                                                    
  if (NS_FAILED(rv)) return rv;

  nsXPIDLCString previous;

  char* iidString = NS_GET_IID(mozISecurebrowser).ToString();

  if (!iidString) return NS_ERROR_OUT_OF_MEMORY;
                                                                                                    
  rv = catman->AddCategoryEntry(JAVASCRIPT_DOM_INTERFACE,
                                "mozISecurebrowser",
                                iidString,
                                PR_TRUE, PR_TRUE, getter_Copies(previous));
  nsCRT::free(iidString);
  NS_ENSURE_SUCCESS(rv, rv);

  return rv;
}


static const nsModuleComponentInfo components[] =
{
  { "Secure Browser Module", 
    MOZ_SECUREBROWSER_CID, 
    "@mozilla.org/securebrowser;1",
    mozSecurebrowserConstructor,
    RegisterRuntime
  }
};

void PR_CALLBACK
RuntimeDestructor(nsIModule* self)
{
}

NS_IMPL_NSGETMODULE_WITH_DTOR(mozSecurebrowserModule, components, RuntimeDestructor)
********/

