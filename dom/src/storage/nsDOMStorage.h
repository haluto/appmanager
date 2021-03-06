/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMStorage_h___
#define nsDOMStorage_h___

#include "nscore.h"
#include "nsAutoPtr.h"
#include "nsIDOMStorageObsolete.h"
#include "nsIDOMStorage.h"
#include "nsIDOMStorageItem.h"
#include "nsIPermissionManager.h"
#include "nsIPrivacyTransitionObserver.h"
#include "nsInterfaceHashtable.h"
#include "nsVoidArray.h"
#include "nsTArray.h"
#include "nsPIDOMStorage.h"
#include "nsIDOMToString.h"
#include "nsDOMEvent.h"
#include "nsIDOMStorageEvent.h"
#include "nsIDOMStorageManager.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIObserver.h"
#include "nsITimer.h"
#include "nsWeakReference.h"
#include "nsIInterfaceRequestor.h"
#include "mozilla/Attributes.h"

#include "nsDOMStorageDBWrapper.h"

class nsDOMStorage;
class nsIDOMStorage;
class nsDOMStorageItem;
class nsDOMStoragePersistentDB;

namespace mozilla {
namespace dom {
class StorageParent;
}
}
using mozilla::dom::StorageParent;

class DOMStorageImpl;

// Only broadcast value changes below 500K.
static const uint32_t MAX_VALUE_BROADCAST_SIZE = 512 * 1024;

class nsDOMStorageEntry : public nsPtrHashKey<const void>
{
public:
  nsDOMStorageEntry(KeyTypePointer aStr);
  nsDOMStorageEntry(const nsDOMStorageEntry& aToCopy);
  ~nsDOMStorageEntry();

  // weak reference so that it can be deleted when no longer used
  DOMStorageImpl* mStorage;
};

class nsSessionStorageEntry : public nsCStringHashKey
{
public:
  nsSessionStorageEntry(KeyTypePointer aStr);
  nsSessionStorageEntry(const nsSessionStorageEntry& aToCopy);
  ~nsSessionStorageEntry();

  nsRefPtr<nsDOMStorageItem> mItem;
};

class nsDOMStorageManager MOZ_FINAL : public nsIDOMStorageManager
                                    , public nsIObserver
                                    , public nsSupportsWeakReference
{
public:
  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMStorageManager
  NS_DECL_NSIDOMSTORAGEMANAGER

  // nsIObserver
  NS_DECL_NSIOBSERVER

  nsDOMStorageManager();

  void AddToStoragesHash(DOMStorageImpl* aStorage);
  void RemoveFromStoragesHash(DOMStorageImpl* aStorage);

  nsresult ClearAllStorages();

  static nsresult Initialize();
  static nsDOMStorageManager* GetInstance();
  static void Shutdown();
  static void ShutdownDB();

  /**
   * Checks whether there is any data waiting to be flushed from a temp table.
   */
  bool UnflushedDataExists();

  static nsDOMStorageManager* gStorageManager;

protected:

  nsTHashtable<nsDOMStorageEntry> mStorages;
};

class DOMStorageBase : public nsIPrivacyTransitionObserver
{
public:
  DOMStorageBase();
  DOMStorageBase(DOMStorageBase&);

  virtual void InitAsSessionStorage(nsIPrincipal* aPrincipal, bool aPrivate);
  virtual void InitAsLocalStorage(nsIPrincipal* aPrincipal, bool aPrivate);

  virtual nsTArray<nsCString>* GetKeys(bool aCallerSecure) = 0;
  virtual nsresult GetLength(bool aCallerSecure, uint32_t* aLength) = 0;
  virtual nsresult GetKey(bool aCallerSecure, uint32_t aIndex, nsACString& aKey) = 0;
  virtual nsIDOMStorageItem* GetValue(bool aCallerSecure, const nsACString& aKey,
                                      nsresult* rv) = 0;
  virtual nsresult SetValue(bool aCallerSecure, const nsACString& aKey,
                            const nsACString& aData, nsACString& aOldValue) = 0;
  virtual nsresult RemoveValue(bool aCallerSecure, const nsACString& aKey,
                               nsACString& aOldValue) = 0;
  virtual nsresult Clear(bool aCallerSecure, int32_t* aOldCount) = 0;

  // Call nsDOMStorage::CanUseStorage with |this|
  bool CanUseStorage();

  virtual void MarkOwnerDead() {}

  // If true, the contents of the storage should be stored in the
  // database, otherwise this storage should act like a session
  // storage.
  // This call relies on mSessionOnly, and should only be used
  // after a CacheStoragePermissions() call.  See the comments
  // for mSessionOnly below.
  bool UseDB() {
    return mUseDB;
  }

  bool IsPrivate() {
    return mInPrivateBrowsing;
  }

  // retrieve the value and secure state corresponding to a key out of storage.
  virtual nsresult
  GetDBValue(const nsACString& aKey,
             nsACString& aValue,
             bool* aSecure) = 0;

  // set the value corresponding to a key in the storage. If
  // aSecure is false, then attempts to modify a secure value
  // throw NS_ERROR_DOM_INVALID_ACCESS_ERR
  virtual nsresult
  SetDBValue(const nsACString& aKey,
             const nsACString& aValue,
             bool aSecure) = 0;

  // set the value corresponding to a key as secure.
  virtual nsresult
  SetSecure(const nsACString& aKey, bool aSecure) = 0;

  virtual nsresult
  CloneFrom(bool aCallerSecure, DOMStorageBase* aThat) = 0;

  // e.g. "moc.rab.oof.:" or "moc.rab.oof.:http:80" depending
  // on association with a domain (globalStorage) or
  // an origin (localStorage).
  nsCString& GetScopeDBKey() {return mScopeDBKey;}

  // e.g. "moc.rab.%" - reversed eTLD+1 subpart of the domain.
  nsCString& GetQuotaDBKey()
  {
    return mQuotaDBKey;
  }

  virtual bool CacheStoragePermissions() = 0;

protected:
  friend class nsDOMStorageManager;
  friend class nsDOMStorage;

  nsPIDOMStorage::nsDOMStorageType mStorageType;
  
  // true if the storage database should be used for values
  bool mUseDB;

  // true if the preferences indicates that this storage should be
  // session only. This member is updated by
  // CacheStoragePermissions(), using the current principal.
  // CacheStoragePermissions() must be called at each entry point to
  // make sure this stays up to date.
  bool mSessionOnly;

  // keys are used for database queries.
  // see comments of the getters bellow.
  nsCString mScopeDBKey;
  nsCString mQuotaDBKey;

  bool mInPrivateBrowsing;
};

class DOMStorageImpl MOZ_FINAL : public DOMStorageBase
                               , public nsSupportsWeakReference
{
public:
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(DOMStorageImpl, nsIPrivacyTransitionObserver)
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSIPRIVACYTRANSITIONOBSERVER

  DOMStorageImpl(nsDOMStorage*);
  DOMStorageImpl(nsDOMStorage*, DOMStorageImpl&);
  ~DOMStorageImpl();

  bool SessionOnly() {
    return mSessionOnly;
  }

  virtual nsTArray<nsCString>* GetKeys(bool aCallerSecure);
  virtual nsresult GetLength(bool aCallerSecure, uint32_t* aLength);
  virtual nsresult GetKey(bool aCallerSecure, uint32_t aIndex, nsACString& aKey);
  virtual nsIDOMStorageItem* GetValue(bool aCallerSecure, const nsACString& aKey,
                                      nsresult* rv);
  virtual nsresult SetValue(bool aCallerSecure, const nsACString& aKey,
                            const nsACString& aData, nsACString& aOldValue);
  virtual nsresult RemoveValue(bool aCallerSecure, const nsACString& aKey,
                               nsACString& aOldValue);
  virtual nsresult Clear(bool aCallerSecure, int32_t* aOldCount);

  // cache the keys from the database for faster lookup
  nsresult CacheKeysFromDB();

  uint64_t CachedVersion() { return mItemsCachedVersion; }
  void SetCachedVersion(uint64_t version) { mItemsCachedVersion = version; }
  
  // retrieve the value and secure state corresponding to a key out of storage
  // that has been cached in mItems hash table.
  nsresult
  GetCachedValue(const nsACString& aKey,
                 nsACString& aValue,
                 bool* aSecure);

  // retrieve the value and secure state corresponding to a key out of storage.
  virtual nsresult
  GetDBValue(const nsACString& aKey,
             nsACString& aValue,
             bool* aSecure);

  // set the value corresponding to a key in the storage. If
  // aSecure is false, then attempts to modify a secure value
  // throw NS_ERROR_DOM_INVALID_ACCESS_ERR
  virtual nsresult
  SetDBValue(const nsACString& aKey,
             const nsACString& aValue,
             bool aSecure);

  // set the value corresponding to a key as secure.
  virtual nsresult
  SetSecure(const nsACString& aKey, bool aSecure);

  // clear all values from the store
  void ClearAll();

  virtual nsresult
  CloneFrom(bool aCallerSecure, DOMStorageBase* aThat);

  virtual bool CacheStoragePermissions();

private:
  static nsDOMStorageDBWrapper* gStorageDB;
  friend class nsDOMStorageManager;
  friend class nsDOMStoragePersistentDB;
  friend class StorageParent;

  void Init(nsDOMStorage*);

  // Cross-process storage implementations never have InitAs(Session|Local|Global)Storage
  // called, so the appropriate initialization needs to happen from the child.
  void InitFromChild(bool aUseDB, bool aSessionOnly,
                     bool aPrivate,
                     const nsACString& aScopeDBKey,
                     const nsACString& aQuotaDBKey,
                     uint32_t aStorageType);
  void SetSessionOnly(bool aSessionOnly);

  static nsresult InitDB();

  // 0 initially or a positive data version number assigned by gStorageDB
  // after keys have been cached from the database
  uint64_t mItemsCachedVersion;

  // the key->value item pairs
  nsTHashtable<nsSessionStorageEntry> mItems;

  // Weak reference to the owning storage instance
  nsDOMStorage* mOwner;
};

class nsDOMStorage2;

class nsDOMStorage : public nsIDOMStorageObsolete,
                     public nsPIDOMStorage,
                     public nsIInterfaceRequestor
{
public:
  nsDOMStorage();
  nsDOMStorage(nsDOMStorage& aThat);
  virtual ~nsDOMStorage();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsDOMStorage, nsIDOMStorageObsolete)

  NS_DECL_NSIDOMSTORAGEOBSOLETE
  NS_DECL_NSIINTERFACEREQUESTOR

  // Helpers for implementing nsIDOMStorage
  nsresult GetItem(const nsACString& key, nsACString& aData);
  nsresult Clear();

  void MarkOwnerDead();

  // nsPIDOMStorage
  virtual nsresult InitAsSessionStorage(nsIPrincipal *aPrincipal, const nsSubstring &aDocumentURI,
                                        bool aPrivate);
  virtual nsresult InitAsLocalStorage(nsIPrincipal *aPrincipal, const nsSubstring &aDocumentURI,
                                      bool aPrivate);
  virtual already_AddRefed<nsIDOMStorage> Clone();
  virtual already_AddRefed<nsIDOMStorage> Fork(const nsSubstring &aDocumentURI);
  virtual bool IsForkOf(nsIDOMStorage* aThat);
  virtual nsTArray<nsCString> *GetKeys();
  virtual nsIPrincipal* Principal();
  virtual bool CanAccess(nsIPrincipal *aPrincipal);
  virtual nsDOMStorageType StorageType();

  // Check whether storage may be used by the caller, and whether it
  // is session only.  Returns true if storage may be used.
  static bool
  CanUseStorage(DOMStorageBase* aStorage = nullptr);

  // Check whether storage may be used.  Updates mSessionOnly based on
  // the result of CanUseStorage.
  bool
  CacheStoragePermissions();

  nsIDOMStorageItem* GetNamedItem(const nsACString& aKey, nsresult* aResult);

  nsresult SetSecure(const nsACString& aKey, bool aSecure)
  {
    return mStorageImpl->SetSecure(aKey, aSecure);
  }

  nsresult CloneFrom(nsDOMStorage* aThat);

 protected:
  friend class nsDOMStorage2;
  friend class nsDOMStoragePersistentDB;

  nsRefPtr<DOMStorageBase> mStorageImpl;

  bool CanAccessSystem(nsIPrincipal *aPrincipal);

  // document URI of the document this storage is bound to
  nsString mDocumentURI;

  // true if this storage was initialized as a localStorage object.  localStorage
  // objects are scoped to scheme/host/port in the database, while globalStorage
  // objects are scoped just to host.  this flag also tells the manager to map
  // this storage also in mLocalStorages hash table.
  nsDOMStorageType mStorageType;

  friend class nsIDOMStorage2;
  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsDOMStorage2* mEventBroadcaster;
};

class nsDOMStorage2 MOZ_FINAL : public nsIDOMStorage,
                                public nsPIDOMStorage,
                                public nsIInterfaceRequestor
{
public:
  // nsISupports
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsDOMStorage2, nsIDOMStorage)

  nsDOMStorage2(nsDOMStorage2& aThat);
  nsDOMStorage2();

  NS_DECL_NSIDOMSTORAGE
  NS_DECL_NSIINTERFACEREQUESTOR

  // nsPIDOMStorage
  virtual nsresult InitAsSessionStorage(nsIPrincipal *aPrincipal, const nsSubstring &aDocumentURI,
                                        bool aPrivate);
  virtual nsresult InitAsLocalStorage(nsIPrincipal *aPrincipal, const nsSubstring &aDocumentURI,
                                      bool aPrivate);
  virtual already_AddRefed<nsIDOMStorage> Clone();
  virtual already_AddRefed<nsIDOMStorage> Fork(const nsSubstring &aDocumentURI);
  virtual bool IsForkOf(nsIDOMStorage* aThat);
  virtual nsTArray<nsCString> *GetKeys();
  virtual nsIPrincipal* Principal();
  virtual bool CanAccess(nsIPrincipal *aPrincipal);
  virtual nsDOMStorageType StorageType();

  void BroadcastChangeNotification(const nsCSubstring &aKey,
                                   const nsCSubstring &aOldValue,
                                   const nsCSubstring &aNewValue);
  void InitAsSessionStorageFork(nsIPrincipal *aPrincipal,
                                const nsSubstring &aDocumentURI,
                                nsDOMStorage* aStorage);

private:
  // storages bound to an origin hold the principal to
  // make security checks against it
  nsCOMPtr<nsIPrincipal> mPrincipal;

  // Needed for the storage event, this is address of the document this storage
  // is bound to
  nsString mDocumentURI;
  nsRefPtr<nsDOMStorage> mStorage;
};

class nsDOMStorageItem : public nsIDOMStorageItem,
                         public nsIDOMToString
{
public:
  nsDOMStorageItem(DOMStorageBase* aStorage,
                   const nsACString& aKey,
                   const nsACString& aValue,
                   bool aSecure);
  virtual ~nsDOMStorageItem();

  // nsISupports
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsDOMStorageItem, nsIDOMStorageItem)

  // nsIDOMStorageObsolete
  NS_DECL_NSIDOMSTORAGEITEM

  // nsIDOMToString
  NS_DECL_NSIDOMTOSTRING

  nsresult GetValueNoConvert(nsACString& aValue);

  bool IsSecure()
  {
    return mSecure;
  }

  void SetSecureInternal(bool aSecure)
  {
    mSecure = aSecure;
  }

  const nsACString& GetValueInternal()
  {
    return mValue;
  }

  const void SetValueInternal(const nsACString& aValue)
  {
    mValue = aValue;
  }

  void ClearValue()
  {
    mValue.Truncate();
  }

protected:

  // true if this value is for secure sites only
  bool mSecure;

  // key for the item
  nsCString mKey;

  // value of the item
  nsCString mValue;

  // If this item came from the db, mStorage points to the storage
  // object where this item came from.
  nsRefPtr<DOMStorageBase> mStorage;
};

nsresult
NS_NewDOMStorage2(nsISupports* aOuter, REFNSIID aIID, void** aResult);

#endif /* nsDOMStorage_h___ */
