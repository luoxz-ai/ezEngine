#pragma once

#include <EditorFramework/Assets/AssetDocumentManager.h>

class ezVisualScriptClassAssetManager : public ezAssetDocumentManager
{
  EZ_ADD_DYNAMIC_REFLECTION(ezVisualScriptClassAssetManager, ezAssetDocumentManager);

public:
  ezVisualScriptClassAssetManager();
  ~ezVisualScriptClassAssetManager();

private:
  void OnDocumentManagerEvent(const ezDocumentManager::Event& e);

  virtual void InternalCreateDocument(
    const char* szDocumentTypeName, const char* szPath, bool bCreateNewDocument, ezDocument*& out_pDocument, const ezDocumentObject* pOpenContext) override;
  virtual void InternalGetSupportedDocumentTypes(ezDynamicArray<const ezDocumentTypeDescriptor*>& inout_DocumentTypes) const override;

  virtual bool GeneratesProfileSpecificAssets() const override { return false; }

  ezAssetDocumentTypeDescriptor m_DocTypeDesc;
};
