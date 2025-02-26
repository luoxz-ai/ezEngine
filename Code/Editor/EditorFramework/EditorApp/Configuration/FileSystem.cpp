#include <EditorFramework/EditorFrameworkPCH.h>

#include <EditorFramework/Assets/AssetCurator.h>
#include <EditorFramework/EditorApp/EditorApp.moc.h>
#include <Foundation/IO/OSFile.h>

void ezQtEditorApp::AddPluginDataDirDependency(const char* szSdkRootRelativePath, const char* szRootName, bool bWriteable)
{
  ezStringBuilder sPath = szSdkRootRelativePath;
  sPath.MakeCleanPath();

  for (auto& dd : m_FileSystemConfig.m_DataDirs)
  {
    if (dd.m_sDataDirSpecialPath == sPath)
    {
      dd.m_bHardCodedDependency = true;

      if (bWriteable)
        dd.m_bWritable = true;

      return;
    }
  }

  ezApplicationFileSystemConfig::DataDirConfig cfg;
  cfg.m_sDataDirSpecialPath = sPath;
  cfg.m_bWritable = bWriteable;
  cfg.m_sRootName = szRootName;
  cfg.m_bHardCodedDependency = true;

  m_FileSystemConfig.m_DataDirs.PushBack(cfg);
}

void ezQtEditorApp::SetFileSystemConfig(const ezApplicationFileSystemConfig& cfg)
{
  if (m_FileSystemConfig == cfg)
    return;

  m_FileSystemConfig = cfg;
  ezQtEditorApp::GetSingleton()->AddReloadProjectRequiredReason("The data directory configuration has changed.");

  m_FileSystemConfig.CreateDataDirStubFiles().IgnoreResult();
}

void ezQtEditorApp::SetupDataDirectories()
{
  EZ_PROFILE_SCOPE("SetupDataDirectories");
  ezFileSystem::DetectSdkRootDirectory().IgnoreResult();

  ezStringBuilder sPath = ezToolsProject::GetSingleton()->GetProjectDirectory();

  ezFileSystem::SetSpecialDirectory("project", sPath);

  sPath.AppendPath("RuntimeConfigs/DataDirectories.ddl");
  // we cannot use the default ":project/" path here, because that data directory will only be configured a few lines below
  // so instead we use the absolute path directly
  m_FileSystemConfig.Load(sPath);

  ezEditorAppEvent e;
  e.m_Type = ezEditorAppEvent::Type::BeforeApplyDataDirectories;
  m_Events.Broadcast(e);

  ezQtEditorApp::GetSingleton()->AddPluginDataDirDependency(">sdk/Data/Base", "base", false);
  ezQtEditorApp::GetSingleton()->AddPluginDataDirDependency(">sdk/Data/Plugins", "plugins", false);
  ezQtEditorApp::GetSingleton()->AddPluginDataDirDependency(">project/", "project", true);

  // Tell the tools project that all data directories are ok to put documents in
  {
    for (const auto& dd : m_FileSystemConfig.m_DataDirs)
    {
      if (ezFileSystem::ResolveSpecialDirectory(dd.m_sDataDirSpecialPath, sPath).Succeeded())
      {
        ezToolsProject::GetSingleton()->AddAllowedDocumentRoot(sPath);
      }
    }
  }

  m_FileSystemConfig.Apply();
}

bool ezQtEditorApp::MakeParentDataDirectoryRelativePathAbsolute(ezStringBuilder& ref_sPath, bool bCheckExists) const
{
  ref_sPath.MakeCleanPath();

  if (ezPathUtils::IsAbsolutePath(ref_sPath))
    return true;

  if (ezPathUtils::IsRootedPath(ref_sPath))
  {
    ezStringBuilder sAbsPath;
    if (ezFileSystem::ResolvePath(ref_sPath, &sAbsPath, nullptr).Succeeded())
    {
      ref_sPath = sAbsPath;
      return true;
    }

    return false;
  }

  if (ezConversionUtils::IsStringUuid(ref_sPath))
  {
    ezUuid guid = ezConversionUtils::ConvertStringToUuid(ref_sPath);
    auto pAsset = ezAssetCurator::GetSingleton()->GetSubAsset(guid);

    if (pAsset == nullptr)
      return false;

    ref_sPath = pAsset->m_pAssetInfo->m_sAbsolutePath;
    return true;
  }

  ezStringBuilder sTemp, sFolder, sDataDirName;

  const char* szEnd = ref_sPath.FindSubString("/");
  if (szEnd)
  {
    sDataDirName.SetSubString_FromTo(ref_sPath.GetData(), szEnd);
  }
  else
  {
    sDataDirName = ref_sPath;
  }

  for (ezUInt32 i = m_FileSystemConfig.m_DataDirs.GetCount(); i > 0; --i)
  {
    const auto& dd = m_FileSystemConfig.m_DataDirs[i - 1];

    if (ezFileSystem::ResolveSpecialDirectory(dd.m_sDataDirSpecialPath, sTemp).Failed())
      continue;

    // only check data directories that start with the required name
    while (sTemp.EndsWith("/") || sTemp.EndsWith("\\"))
      sTemp.Shrink(0, 1);
    const ezStringView folderName = sTemp.GetFileName();

    if (sDataDirName != folderName)
      continue;

    sTemp.PathParentDirectory(); // the secret sauce is here
    sTemp.AppendPath(ref_sPath);
    sTemp.MakeCleanPath();

    if (!bCheckExists || ezOSFile::ExistsFile(sTemp) || ezOSFile::ExistsDirectory(sTemp))
    {
      ref_sPath = sTemp;
      return true;
    }
  }

  return false;
}

bool ezQtEditorApp::MakeDataDirectoryRelativePathAbsolute(ezStringBuilder& ref_sPath) const
{
  if (ezPathUtils::IsAbsolutePath(ref_sPath))
    return true;

  if (ezPathUtils::IsRootedPath(ref_sPath))
  {
    ezStringBuilder sAbsPath;
    if (ezFileSystem::ResolvePath(ref_sPath, &sAbsPath, nullptr).Succeeded())
    {
      ref_sPath = sAbsPath;
      return true;
    }

    return false;
  }

  if (ezConversionUtils::IsStringUuid(ref_sPath))
  {
    ezUuid guid = ezConversionUtils::ConvertStringToUuid(ref_sPath);
    auto pAsset = ezAssetCurator::GetSingleton()->GetSubAsset(guid);

    if (pAsset == nullptr)
      return false;

    ref_sPath = pAsset->m_pAssetInfo->m_sAbsolutePath;
    return true;
  }

  ezStringBuilder sTemp;

  for (ezUInt32 i = m_FileSystemConfig.m_DataDirs.GetCount(); i > 0; --i)
  {
    const auto& dd = m_FileSystemConfig.m_DataDirs[i - 1];

    if (ezFileSystem::ResolveSpecialDirectory(dd.m_sDataDirSpecialPath, sTemp).Failed())
      continue;

    sTemp.AppendPath(ref_sPath);
    sTemp.MakeCleanPath();

    if (ezOSFile::ExistsFile(sTemp) || ezOSFile::ExistsDirectory(sTemp))
    {
      ref_sPath = sTemp;
      return true;
    }
  }

  return false;
}

bool ezQtEditorApp::MakeDataDirectoryRelativePathAbsolute(ezString& ref_sPath) const
{
  ezStringBuilder sTemp = ref_sPath;
  bool bRes = MakeDataDirectoryRelativePathAbsolute(sTemp);
  ref_sPath = sTemp;
  return bRes;
}

bool ezQtEditorApp::MakePathDataDirectoryRelative(ezStringBuilder& ref_sPath) const
{
  ezStringBuilder sTemp;

  for (ezUInt32 i = m_FileSystemConfig.m_DataDirs.GetCount(); i > 0; --i)
  {
    const auto& dd = m_FileSystemConfig.m_DataDirs[i - 1];

    if (ezFileSystem::ResolveSpecialDirectory(dd.m_sDataDirSpecialPath, sTemp).Failed())
      continue;

    if (ref_sPath.IsPathBelowFolder(sTemp))
    {
      ref_sPath.MakeRelativeTo(sTemp).IgnoreResult();
      return true;
    }
  }

  ref_sPath.MakeRelativeTo(ezFileSystem::GetSdkRootDirectory()).IgnoreResult();
  return false;
}

bool ezQtEditorApp::MakePathDataDirectoryParentRelative(ezStringBuilder& ref_sPath) const
{
  ezStringBuilder sTemp;

  for (ezUInt32 i = m_FileSystemConfig.m_DataDirs.GetCount(); i > 0; --i)
  {
    const auto& dd = m_FileSystemConfig.m_DataDirs[i - 1];

    if (ezFileSystem::ResolveSpecialDirectory(dd.m_sDataDirSpecialPath, sTemp).Failed())
      continue;

    if (ref_sPath.IsPathBelowFolder(sTemp))
    {
      sTemp.PathParentDirectory();

      ref_sPath.MakeRelativeTo(sTemp).IgnoreResult();
      return true;
    }
  }

  ref_sPath.MakeRelativeTo(ezFileSystem::GetSdkRootDirectory()).IgnoreResult();
  return false;
}

bool ezQtEditorApp::MakePathDataDirectoryRelative(ezString& ref_sPath) const
{
  ezStringBuilder sTemp = ref_sPath;
  bool bRes = MakePathDataDirectoryRelative(sTemp);
  ref_sPath = sTemp;
  return bRes;
}
