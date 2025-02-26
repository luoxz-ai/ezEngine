#include <ToolsFoundationTest/ToolsFoundationTestPCH.h>

#if EZ_ENABLED(EZ_SUPPORTS_DIRECTORY_WATCHER) && EZ_ENABLED(EZ_SUPPORTS_FILE_ITERATORS)

#  include <Foundation/Application/Config/FileSystemConfig.h>
#  include <Foundation/IO/FileSystem/DataDirTypeFolder.h>
#  include <Foundation/IO/FileSystem/FileReader.h>
#  include <Foundation/IO/FileSystem/FileSystem.h>
#  include <Foundation/IO/FileSystem/FileWriter.h>
#  include <Foundation/Threading/ThreadUtils.h>
#  include <ToolsFoundation/FileSystem/FileSystemModel.h>


EZ_CREATE_SIMPLE_TEST_GROUP(FileSystem);

namespace
{
  ezResult eztCreateFile(ezStringView sPath)
  {
    ezFileWriter FileOut;
    EZ_SUCCEED_OR_RETURN(FileOut.Open(sPath));
    EZ_SUCCEED_OR_RETURN(FileOut.WriteString("Test"));
    FileOut.Close();
    return EZ_SUCCESS;
  }
} // namespace

EZ_CREATE_SIMPLE_TEST(FileSystem, FileSystemModel)
{
  constexpr ezUInt32 WAIT_LOOPS = 1000;

  ezStringBuilder sOutputFolder = ezTestFramework::GetInstance()->GetAbsOutputPath();
  sOutputFolder.AppendPath("Model");
  sOutputFolder.MakeCleanPath();

  ezStringBuilder sOutputFolderResolved;
  ezFileSystem::ResolveSpecialDirectory(sOutputFolder, sOutputFolderResolved).IgnoreResult();

  ezApplicationFileSystemConfig fsConfig;
  ezApplicationFileSystemConfig::DataDirConfig& dataDir = fsConfig.m_DataDirs.ExpandAndGetRef();
  dataDir.m_bWritable = true;
  dataDir.m_sDataDirSpecialPath = sOutputFolder;
  dataDir.m_sRootName = "output";

  // Files
  ezHybridArray<ezFileChangedEvent, 2> fileEvents;
  ezHybridArray<ezTime, 2> fileEventTimestamps;
  ezMutex fileEventLock;
  auto fileEvent = [&](const ezFileChangedEvent& e) {
    EZ_LOCK(fileEventLock);
    fileEvents.PushBack(e);
    fileEventTimestamps.PushBack(ezTime::Now());

    ezFileStatus stat;
    switch (e.m_Type)
    {
      case ezFileChangedEvent::Type::FileRemoved:
        EZ_TEST_BOOL(ezFileSystemModel::GetSingleton()->FindFile(e.m_sPath, stat).Failed());
        break;
      case ezFileChangedEvent::Type::FileAdded:
      case ezFileChangedEvent::Type::FileChanged:
      case ezFileChangedEvent::Type::DocumentLinked:
        EZ_TEST_BOOL(ezFileSystemModel::GetSingleton()->FindFile(e.m_sPath, stat).Succeeded());
        break;

      case ezFileChangedEvent::Type::ModelReset:
      default:
        break;
    }
  };
  ezEventSubscriptionID fileId = ezFileSystemModel::GetSingleton()->m_FileChangedEvents.AddEventHandler(fileEvent);

  // Folders
  ezHybridArray<ezFolderChangedEvent, 2> folderEvents;
  ezHybridArray<ezTime, 2> folderEventTimestamps;
  ezMutex folderEventLock;
  auto folderEvent = [&](const ezFolderChangedEvent& e) {
    EZ_LOCK(folderEventLock);
    folderEvents.PushBack(e);
    folderEventTimestamps.PushBack(ezTime::Now());

    switch (e.m_Type)
    {
      case ezFolderChangedEvent::Type::FolderAdded:
        EZ_TEST_BOOL(ezFileSystemModel::GetSingleton()->GetFolders()->Contains(e.m_sPath));
        break;
      case ezFolderChangedEvent::Type::FolderRemoved:
        EZ_TEST_BOOL(!ezFileSystemModel::GetSingleton()->GetFolders()->Contains(e.m_sPath));
        break;
      case ezFolderChangedEvent::Type::ModelReset:
      default:
        break;
    }
  };
  ezEventSubscriptionID folderId = ezFileSystemModel::GetSingleton()->m_FolderChangedEvents.AddEventHandler(folderEvent);

  // Helper functions
  auto CompareFiles = [&](ezArrayPtr<ezFileChangedEvent> expected) {
    EZ_LOCK(fileEventLock);
    if (EZ_TEST_INT(expected.GetCount(), fileEvents.GetCount()))
    {
      for (size_t i = 0; i < expected.GetCount(); i++)
      {
        EZ_TEST_INT((int)expected[i].m_Type, (int)fileEvents[i].m_Type);
        EZ_TEST_STRING(expected[i].m_sPath, fileEvents[i].m_sPath);
        // Ignore stats
      }
    }
  };

  auto ClearFiles = [&]() {
    EZ_LOCK(fileEventLock);
    fileEvents.Clear();
    fileEventTimestamps.Clear();
  };

  auto CompareFolders = [&](ezArrayPtr<ezFolderChangedEvent> expected) {
    EZ_LOCK(folderEventLock);
    if (EZ_TEST_INT(expected.GetCount(), folderEvents.GetCount()))
    {
      for (size_t i = 0; i < expected.GetCount(); i++)
      {
        EZ_TEST_INT((int)expected[i].m_Type, (int)folderEvents[i].m_Type);
        EZ_TEST_STRING(expected[i].m_sPath, folderEvents[i].m_sPath);
        // Ignore stats
      }
    }
  };

  auto ClearFolders = [&]() {
    EZ_LOCK(folderEventLock);
    folderEvents.Clear();
    folderEventTimestamps.Clear();
  };


  EZ_TEST_BLOCK(ezTestBlock::Enabled, "Startup")
  {
    ezFileSystem::RegisterDataDirectoryFactory(ezDataDirectory::FolderType::Factory);

    EZ_TEST_RESULT(ezOSFile::DeleteFolder(sOutputFolderResolved));
    EZ_TEST_RESULT(ezFileSystem::CreateDirectoryStructure(sOutputFolderResolved));

    // for absolute paths
    EZ_TEST_BOOL(ezFileSystem::AddDataDirectory("", "", ":", ezFileSystem::AllowWrites) == EZ_SUCCESS);
    EZ_TEST_BOOL(ezFileSystem::AddDataDirectory(sOutputFolder, "Clear", "output", ezFileSystem::AllowWrites) == EZ_SUCCESS);

    ezFileSystemModel::GetSingleton()->Initialize(fsConfig, {}, {});

    ezFileChangedEvent expected[] = {ezFileChangedEvent({}, {}, ezFileChangedEvent::Type::ModelReset)};
    CompareFiles(ezMakeArrayPtr(expected));
    ClearFiles();

    ezFolderChangedEvent expected2[] = {ezFolderChangedEvent({}, ezFolderChangedEvent::Type::ModelReset)};
    CompareFolders(ezMakeArrayPtr(expected2));
    ClearFolders();

    EZ_TEST_INT(ezFileSystemModel::GetSingleton()->GetFiles()->GetCount(), 0);
    EZ_TEST_INT(ezFileSystemModel::GetSingleton()->GetFolders()->GetCount(), 1);

    auto it = ezFileSystemModel::GetSingleton()->GetFolders()->GetIterator();
    EZ_TEST_STRING(it.Key(), sOutputFolder);
    EZ_TEST_BOOL(it.Value() == ezFileStatus::Status::Valid);
  }

  EZ_TEST_BLOCK(ezTestBlock::Enabled, "Add file")
  {
    ezStringBuilder sFilePath(sOutputFolder);
    sFilePath.AppendPath("rootFile.txt");

    EZ_TEST_RESULT(eztCreateFile(sFilePath));

    for (ezUInt32 i = 0; i < WAIT_LOOPS; i++)
    {
      ezFileSystemModel::GetSingleton()->MainThreadTick();
      ezThreadUtils::Sleep(ezTime::MakeFromMilliseconds(10));

      EZ_LOCK(fileEventLock);
      if (fileEvents.GetCount() > 0)
        break;
    }

    ezFileChangedEvent expected[] = {ezFileChangedEvent(sFilePath, {}, ezFileChangedEvent::Type::FileAdded)};
    CompareFiles(ezMakeArrayPtr(expected));
    ClearFiles();
    CompareFolders({});

    EZ_TEST_INT(ezFileSystemModel::GetSingleton()->GetFiles()->GetCount(), 1);
    EZ_TEST_INT(ezFileSystemModel::GetSingleton()->GetFolders()->GetCount(), 1);
  }

  EZ_TEST_BLOCK(ezTestBlock::Enabled, "modify file")
  {
    ezStringBuilder sFilePath(sOutputFolder);
    sFilePath.AppendPath("rootFile.txt");

    {
#  if EZ_ENABLED(EZ_PLATFORM_LINUX)
      // EXT3 filesystem only support second resolution so we won't detect the modification if it is done within the same second.
      ezThreadUtils::Sleep(ezTime::MakeFromSeconds(1.0));
#  endif
      ezFileWriter FileOut;
      EZ_TEST_RESULT(FileOut.Open(sFilePath));
      EZ_TEST_RESULT(FileOut.WriteString("Test2"));
      EZ_TEST_RESULT(FileOut.Flush());
      FileOut.Close();
    }

    for (ezUInt32 i = 0; i < WAIT_LOOPS; i++)
    {
      ezFileSystemModel::GetSingleton()->MainThreadTick();
      ezThreadUtils::Sleep(ezTime::MakeFromMilliseconds(10));

      EZ_LOCK(fileEventLock);
      if (fileEvents.GetCount() > 0)
        break;
    }

    ezFileChangedEvent expected[] = {ezFileChangedEvent(sFilePath, {}, ezFileChangedEvent::Type::FileChanged)};
    CompareFiles(ezMakeArrayPtr(expected));
    ClearFiles();
    CompareFolders({});

    EZ_TEST_INT(ezFileSystemModel::GetSingleton()->GetFiles()->GetCount(), 1);
    EZ_TEST_INT(ezFileSystemModel::GetSingleton()->GetFolders()->GetCount(), 1);
  }

  EZ_TEST_BLOCK(ezTestBlock::Enabled, "rename file")
  {
    ezStringBuilder sFilePathOld(sOutputFolder);
    sFilePathOld.AppendPath("rootFile.txt");

    ezStringBuilder sFilePathNew(sOutputFolder);
    sFilePathNew.AppendPath("rootFile2.txt");

    EZ_TEST_RESULT(ezOSFile::MoveFileOrDirectory(sFilePathOld, sFilePathNew));

    for (ezUInt32 i = 0; i < WAIT_LOOPS; i++)
    {
      ezFileSystemModel::GetSingleton()->MainThreadTick();
      ezThreadUtils::Sleep(ezTime::MakeFromMilliseconds(10));

      EZ_LOCK(fileEventLock);
      if (fileEvents.GetCount() == 2)
        break;
    }

    ezFileChangedEvent expected[] = {
      ezFileChangedEvent(sFilePathNew, {}, ezFileChangedEvent::Type::FileAdded),
      ezFileChangedEvent(sFilePathOld, {}, ezFileChangedEvent::Type::FileRemoved)};
    CompareFiles(ezMakeArrayPtr(expected));
    ClearFiles();
    CompareFolders({});

    EZ_TEST_INT(ezFileSystemModel::GetSingleton()->GetFiles()->GetCount(), 1);
    EZ_TEST_INT(ezFileSystemModel::GetSingleton()->GetFolders()->GetCount(), 1);
  }

  EZ_TEST_BLOCK(ezTestBlock::Enabled, "Add folder")
  {
    ezStringBuilder sFolderPath(sOutputFolder);
    sFolderPath.AppendPath("Folder1");

    EZ_TEST_RESULT(ezFileSystem::CreateDirectoryStructure(sFolderPath));

    for (ezUInt32 i = 0; i < WAIT_LOOPS; i++)
    {
      ezFileSystemModel::GetSingleton()->MainThreadTick();
      ezThreadUtils::Sleep(ezTime::MakeFromMilliseconds(10));

      EZ_LOCK(folderEventLock);
      if (folderEvents.GetCount() > 0)
        break;
    }

    ezFolderChangedEvent expected[] = {ezFolderChangedEvent(sFolderPath, ezFolderChangedEvent::Type::FolderAdded)};
    CompareFolders(ezMakeArrayPtr(expected));
    ClearFolders();
    CompareFiles({});

    EZ_TEST_INT(ezFileSystemModel::GetSingleton()->GetFiles()->GetCount(), 1);
    EZ_TEST_INT(ezFileSystemModel::GetSingleton()->GetFolders()->GetCount(), 2);
  }

  EZ_TEST_BLOCK(ezTestBlock::Enabled, "move file")
  {
    ezStringBuilder sFilePathOld(sOutputFolder);
    sFilePathOld.AppendPath("rootFile2.txt");

    ezStringBuilder sFilePathNew(sOutputFolder);
    sFilePathNew.AppendPath("Folder1", "rootFile2.txt");

    EZ_TEST_RESULT(ezOSFile::MoveFileOrDirectory(sFilePathOld, sFilePathNew));

    for (ezUInt32 i = 0; i < WAIT_LOOPS; i++)
    {
      ezFileSystemModel::GetSingleton()->MainThreadTick();
      ezThreadUtils::Sleep(ezTime::MakeFromMilliseconds(10));

      EZ_LOCK(fileEventLock);
      if (fileEvents.GetCount() == 2)
        break;
    }

    ezFileChangedEvent expected[] = {
      ezFileChangedEvent(sFilePathNew, {}, ezFileChangedEvent::Type::FileAdded),
      ezFileChangedEvent(sFilePathOld, {}, ezFileChangedEvent::Type::FileRemoved)};
    CompareFiles(ezMakeArrayPtr(expected));
    ClearFiles();
    CompareFolders({});

    EZ_TEST_INT(ezFileSystemModel::GetSingleton()->GetFiles()->GetCount(), 1);
    EZ_TEST_INT(ezFileSystemModel::GetSingleton()->GetFolders()->GetCount(), 2);
  }

  EZ_TEST_BLOCK(ezTestBlock::Enabled, "move folder")
  {
    ezStringBuilder sFolderPathOld(sOutputFolder);
    sFolderPathOld.AppendPath("Folder1");

    ezStringBuilder sFilePathOld(sOutputFolder);
    sFilePathOld.AppendPath("Folder1", "rootFile2.txt");

    ezStringBuilder sFolderPathNew(sOutputFolder);
    sFolderPathNew.AppendPath("Folder2");

    ezStringBuilder sFilePathNew(sOutputFolder);
    sFilePathNew.AppendPath("Folder2", "rootFile2.txt");

    EZ_TEST_RESULT(ezOSFile::MoveFileOrDirectory(sFolderPathOld, sFolderPathNew));

    for (ezUInt32 i = 0; i < WAIT_LOOPS; i++)
    {
      ezFileSystemModel::GetSingleton()->MainThreadTick();
      ezThreadUtils::Sleep(ezTime::MakeFromMilliseconds(10));

      EZ_LOCK(fileEventLock);
      EZ_LOCK(folderEventLock);
      if (fileEvents.GetCount() == 2 && folderEvents.GetCount() == 2)
        break;
    }

    {
      ezFolderChangedEvent expected[] = {
        ezFolderChangedEvent(sFolderPathNew, ezFolderChangedEvent::Type::FolderAdded),
        ezFolderChangedEvent(sFolderPathOld, ezFolderChangedEvent::Type::FolderRemoved)};
      CompareFolders(ezMakeArrayPtr(expected));
    }

    {
      ezFileChangedEvent expected[] = {
        ezFileChangedEvent(sFilePathNew, {}, ezFileChangedEvent::Type::FileAdded),
        ezFileChangedEvent(sFilePathOld, {}, ezFileChangedEvent::Type::FileRemoved)};
      CompareFiles(ezMakeArrayPtr(expected));
    }
    {
      EZ_LOCK(fileEventLock);
      EZ_LOCK(folderEventLock);
      // Check folder added before file
      EZ_TEST_BOOL(fileEventTimestamps[0] > folderEventTimestamps[0]);
      // Check file removed before folder
      EZ_TEST_BOOL(fileEventTimestamps[1] < folderEventTimestamps[1]);
    }

    ClearFolders();
    ClearFiles();

    EZ_TEST_INT(ezFileSystemModel::GetSingleton()->GetFiles()->GetCount(), 1);
    EZ_TEST_INT(ezFileSystemModel::GetSingleton()->GetFolders()->GetCount(), 2);
  }

  EZ_TEST_BLOCK(ezTestBlock::Enabled, "HashFile")
  {
    ezStringBuilder sFilePathNew(sOutputFolder);
    sFilePathNew.AppendPath("Folder2", "rootFile2.txt");

    ezFileStatus status;
    EZ_TEST_RESULT(ezFileSystemModel::GetSingleton()->HashFile(sFilePathNew, status));
    EZ_TEST_INT((ezInt64)status.m_uiHash, (ezInt64)10983861097202158394u);
  }

  ezMap<ezString, ezFileStatus> referencedFiles;
  ezMap<ezString, ezFileStatus::Status> referencedFolders;

  EZ_TEST_BLOCK(ezTestBlock::Enabled, "Shutdown")
  {
    ezFileSystemModel::GetSingleton()->Deinitialize(&referencedFiles, &referencedFolders);
    ezFileChangedEvent expected[] = {ezFileChangedEvent({}, {}, ezFileChangedEvent::Type::ModelReset)};
    CompareFiles(ezMakeArrayPtr(expected));
    ClearFiles();

    ezFolderChangedEvent expected2[] = {ezFolderChangedEvent({}, ezFolderChangedEvent::Type::ModelReset)};
    CompareFolders(ezMakeArrayPtr(expected2));
    ClearFolders();
  }

  EZ_TEST_BLOCK(ezTestBlock::Enabled, "Startup Restore Model")
  {
    ezFileSystemModel::GetSingleton()->Initialize(fsConfig, std::move(referencedFiles), std::move(referencedFolders));

    ezFileChangedEvent expected[] = {ezFileChangedEvent({}, {}, ezFileChangedEvent::Type::ModelReset)};
    CompareFiles(ezMakeArrayPtr(expected));
    ClearFiles();

    ezFolderChangedEvent expected2[] = {ezFolderChangedEvent({}, ezFolderChangedEvent::Type::ModelReset)};
    CompareFolders(ezMakeArrayPtr(expected2));
    ClearFolders();

    EZ_TEST_INT(ezFileSystemModel::GetSingleton()->GetFiles()->GetCount(), 1);
    EZ_TEST_INT(ezFileSystemModel::GetSingleton()->GetFolders()->GetCount(), 2);
  }

  EZ_TEST_BLOCK(ezTestBlock::Enabled, "GetFiles")
  {
    ezStringBuilder sFilePathNew(sOutputFolder);
    sFilePathNew.AppendPath("Folder2", "rootFile2.txt");

    ezFileSystemModel::LockedFiles files = ezFileSystemModel::GetSingleton()->GetFiles();
    EZ_TEST_INT(files->GetCount(), 1);
    auto it = files->GetIterator();
    EZ_TEST_STRING(it.Key(), sFilePathNew);
    EZ_TEST_BOOL(it.Value().m_LastModified.IsValid());
    EZ_TEST_INT((ezInt64)it.Value().m_uiHash, (ezInt64)10983861097202158394u);
    EZ_TEST_BOOL(it.Value().m_Status == ezFileStatus::Status::Valid);
  }

  EZ_TEST_BLOCK(ezTestBlock::Enabled, "GetFolders")
  {
    ezStringBuilder sFolder(sOutputFolder);
    sFolder.AppendPath("Folder2");

    ezFileSystemModel::LockedFolders folders = ezFileSystemModel::GetSingleton()->GetFolders();
    EZ_TEST_INT(folders->GetCount(), 2);
    auto it = folders->GetIterator();

    // ezMap is sorted so the order is fixed.
    EZ_TEST_STRING(it.Key(), sOutputFolder);
    EZ_TEST_BOOL(it.Value() == ezFileStatus::Status::Valid);

    it.Next();
    EZ_TEST_STRING(it.Key(), sFolder);
    EZ_TEST_BOOL(it.Value() == ezFileStatus::Status::Valid);
  }

  EZ_TEST_BLOCK(ezTestBlock::Enabled, "CheckFileSystem")
  {
    ezStringBuilder sFolderPath(sOutputFolder);
    sFolderPath.AppendPath("Folder2");

    ezStringBuilder sFilePath(sOutputFolder);
    sFilePath.AppendPath("Folder2", "rootFile2.txt");

    ezFileSystemModel::GetSingleton()->CheckFileSystem();

    // #TODO_ASSET This FileChanged should be removed once the model is fixed to no longer require firing this after restoring the model from cache. See comment in ezFileSystemModel::HandleSingleFile.
    ezFileChangedEvent expected[] = {
      ezFileChangedEvent(sFilePath, {}, ezFileChangedEvent::Type::FileChanged),
      ezFileChangedEvent({}, {}, ezFileChangedEvent::Type::ModelReset)};
    CompareFiles(ezMakeArrayPtr(expected));
    ClearFiles();

    ezFolderChangedEvent expected2[] = {ezFolderChangedEvent({}, ezFolderChangedEvent::Type::ModelReset)};
    CompareFolders(ezMakeArrayPtr(expected2));
    ClearFolders();

    EZ_TEST_INT(ezFileSystemModel::GetSingleton()->GetFiles()->GetCount(), 1);
    EZ_TEST_INT(ezFileSystemModel::GetSingleton()->GetFolders()->GetCount(), 2);
  }

  EZ_TEST_BLOCK(ezTestBlock::Enabled, "NotifyOfChange - File")
  {
    ezStringBuilder sFilePath(sOutputFolder);
    sFilePath.AppendPath("rootFile.txt");
    {
      EZ_TEST_RESULT(eztCreateFile(sFilePath));
      ezFileSystemModel::GetSingleton()->NotifyOfChange(sFilePath);

      ezFileChangedEvent expected[] = {ezFileChangedEvent(sFilePath, {}, ezFileChangedEvent::Type::FileAdded)};
      CompareFiles(ezMakeArrayPtr(expected));
      ClearFiles();
      CompareFolders({});
      EZ_TEST_INT(ezFileSystemModel::GetSingleton()->GetFiles()->GetCount(), 2);
      EZ_TEST_INT(ezFileSystemModel::GetSingleton()->GetFolders()->GetCount(), 2);
    }

    {
      EZ_TEST_RESULT(ezOSFile::DeleteFile(sFilePath));
      ezFileSystemModel::GetSingleton()->NotifyOfChange(sFilePath);

      ezFileChangedEvent expected[] = {ezFileChangedEvent(sFilePath, {}, ezFileChangedEvent::Type::FileRemoved)};
      CompareFiles(ezMakeArrayPtr(expected));
      ClearFiles();
      CompareFolders({});
      EZ_TEST_INT(ezFileSystemModel::GetSingleton()->GetFiles()->GetCount(), 1);
      EZ_TEST_INT(ezFileSystemModel::GetSingleton()->GetFolders()->GetCount(), 2);
    }
  }

  EZ_TEST_BLOCK(ezTestBlock::Enabled, "NotifyOfChange - Folder")
  {
    ezStringBuilder sFolderPath(sOutputFolder);
    sFolderPath.AppendPath("AnotherFolder");
    {
      EZ_TEST_RESULT(ezFileSystem::CreateDirectoryStructure(sFolderPath));
      ezFileSystemModel::GetSingleton()->NotifyOfChange(sFolderPath);

      CompareFiles({});
      ClearFiles();
      ezFolderChangedEvent expected[] = {ezFolderChangedEvent(sFolderPath, ezFolderChangedEvent::Type::FolderAdded)};
      CompareFolders(ezMakeArrayPtr(expected));
      ClearFolders();
      EZ_TEST_INT(ezFileSystemModel::GetSingleton()->GetFiles()->GetCount(), 1);
      EZ_TEST_INT(ezFileSystemModel::GetSingleton()->GetFolders()->GetCount(), 3);
    }

    {
      EZ_TEST_RESULT(ezOSFile::DeleteFolder(sFolderPath));
      ezFileSystemModel::GetSingleton()->NotifyOfChange(sFolderPath);

      CompareFiles({});
      ClearFiles();
      ezFolderChangedEvent expected[] = {ezFolderChangedEvent(sFolderPath, ezFolderChangedEvent::Type::FolderRemoved)};
      CompareFolders(ezMakeArrayPtr(expected));
      ClearFolders();
      EZ_TEST_INT(ezFileSystemModel::GetSingleton()->GetFiles()->GetCount(), 1);
      EZ_TEST_INT(ezFileSystemModel::GetSingleton()->GetFolders()->GetCount(), 2);
    }
  }

  EZ_TEST_BLOCK(ezTestBlock::Enabled, "CheckFolder - File")
  {
    ezStringBuilder sFilePath(sOutputFolder);
    sFilePath.AppendPath("Folder2", "subFile.txt");
    {
      EZ_TEST_RESULT(eztCreateFile(sFilePath));
      ezFileSystemModel::GetSingleton()->CheckFolder(sOutputFolder);

      ezFileChangedEvent expected[] = {ezFileChangedEvent(sFilePath, {}, ezFileChangedEvent::Type::FileAdded)};
      CompareFiles(ezMakeArrayPtr(expected));
      ClearFiles();
      CompareFolders({});
      EZ_TEST_INT(ezFileSystemModel::GetSingleton()->GetFiles()->GetCount(), 2);
      EZ_TEST_INT(ezFileSystemModel::GetSingleton()->GetFolders()->GetCount(), 2);
    }

    {
      EZ_TEST_RESULT(ezOSFile::DeleteFile(sFilePath));
      ezFileSystemModel::GetSingleton()->CheckFolder(sOutputFolder);

      ezFileChangedEvent expected[] = {ezFileChangedEvent(sFilePath, {}, ezFileChangedEvent::Type::FileRemoved)};
      CompareFiles(ezMakeArrayPtr(expected));
      ClearFiles();
      CompareFolders({});
      EZ_TEST_INT(ezFileSystemModel::GetSingleton()->GetFiles()->GetCount(), 1);
      EZ_TEST_INT(ezFileSystemModel::GetSingleton()->GetFolders()->GetCount(), 2);
    }
  }

  EZ_TEST_BLOCK(ezTestBlock::Enabled, "CheckFolder - Folder")
  {
    ezStringBuilder sFolderPath(sOutputFolder);
    sFolderPath.AppendPath("YetAnotherFolder");
    ezStringBuilder sFolderSubPath(sOutputFolder);
    sFolderSubPath.AppendPath("YetAnotherFolder", "SubFolder");
    {
      EZ_TEST_RESULT(ezFileSystem::CreateDirectoryStructure(sFolderSubPath));
      ezFileSystemModel::GetSingleton()->CheckFolder(sOutputFolder);

      CompareFiles({});
      ClearFiles();
      ezFolderChangedEvent expected[] = {
        ezFolderChangedEvent(sFolderPath, ezFolderChangedEvent::Type::FolderAdded),
        ezFolderChangedEvent(sFolderSubPath, ezFolderChangedEvent::Type::FolderAdded)};
      CompareFolders(ezMakeArrayPtr(expected));
      ClearFolders();
      EZ_TEST_INT(ezFileSystemModel::GetSingleton()->GetFiles()->GetCount(), 1);
      EZ_TEST_INT(ezFileSystemModel::GetSingleton()->GetFolders()->GetCount(), 4);
    }

    {
      EZ_TEST_RESULT(ezOSFile::DeleteFolder(sFolderPath));
      ezFileSystemModel::GetSingleton()->CheckFolder(sOutputFolder);

      CompareFiles({});
      ClearFiles();
      ezFolderChangedEvent expected[] = {
        ezFolderChangedEvent(sFolderSubPath, ezFolderChangedEvent::Type::FolderRemoved),
        ezFolderChangedEvent(sFolderPath, ezFolderChangedEvent::Type::FolderRemoved)};
      CompareFolders(ezMakeArrayPtr(expected));
      ClearFolders();
      EZ_TEST_INT(ezFileSystemModel::GetSingleton()->GetFiles()->GetCount(), 1);
      EZ_TEST_INT(ezFileSystemModel::GetSingleton()->GetFolders()->GetCount(), 2);
    }
  }

  EZ_TEST_BLOCK(ezTestBlock::Enabled, "ReadDocument")
  {
    ezStringBuilder sFilePathNew(sOutputFolder);
    sFilePathNew.AppendPath("Folder2", "rootFile2.txt");

    auto callback = [](const ezFileStatus& status, ezStreamReader& ref_reader) -> ezUuid {
      EZ_TEST_INT((ezInt64)status.m_uiHash, (ezInt64)10983861097202158394u);
      return ezUuid::MakeUuid();
    };

    EZ_TEST_RESULT(ezFileSystemModel::GetSingleton()->ReadDocument(sFilePathNew, callback));

    ezFileChangedEvent expected[] = {ezFileChangedEvent(sFilePathNew, {}, ezFileChangedEvent::Type::DocumentLinked)};
    CompareFiles(ezMakeArrayPtr(expected));
    ClearFiles();
  }

  EZ_TEST_BLOCK(ezTestBlock::Enabled, "LinkDocument")
  {
    ezStringBuilder sFilePathNew(sOutputFolder);
    sFilePathNew.AppendPath("Folder2", "rootFile2.txt");

    {
      ezUuid guid = ezUuid::MakeUuid();
      EZ_TEST_RESULT(ezFileSystemModel::GetSingleton()->LinkDocument(sFilePathNew, guid));
      EZ_TEST_RESULT(ezFileSystemModel::GetSingleton()->LinkDocument(sFilePathNew, guid));

      ezFileChangedEvent expected[] = {ezFileChangedEvent(sFilePathNew, {}, ezFileChangedEvent::Type::DocumentLinked)};
      CompareFiles(ezMakeArrayPtr(expected));
      ClearFiles();
    }
    {
      EZ_TEST_RESULT(ezFileSystemModel::GetSingleton()->UnlinkDocument(sFilePathNew));
      EZ_TEST_RESULT(ezFileSystemModel::GetSingleton()->UnlinkDocument(sFilePathNew));

      ezFileChangedEvent expected[] = {ezFileChangedEvent(sFilePathNew, {}, ezFileChangedEvent::Type::DocumentUnlinked)};
      CompareFiles(ezMakeArrayPtr(expected));
      ClearFiles();
    }
  }

  EZ_TEST_BLOCK(ezTestBlock::Enabled, "delete folder")
  {
    ezStringBuilder sFolderPath(sOutputFolder);
    sFolderPath.AppendPath("Folder2");

    ezStringBuilder sFilePath(sOutputFolder);
    sFilePath.AppendPath("Folder2", "rootFile2.txt");

    EZ_TEST_RESULT(ezOSFile::DeleteFolder(sFolderPath));

    for (ezUInt32 i = 0; i < WAIT_LOOPS; i++)
    {
      ezFileSystemModel::GetSingleton()->MainThreadTick();
      ezThreadUtils::Sleep(ezTime::MakeFromMilliseconds(10));

      EZ_LOCK(fileEventLock);
      EZ_LOCK(folderEventLock);
      if (fileEvents.GetCount() == 1 && folderEvents.GetCount() == 1)
        break;
    }

    {
      ezFolderChangedEvent expected[] = {
        ezFolderChangedEvent(sFolderPath, ezFolderChangedEvent::Type::FolderRemoved)};
      CompareFolders(ezMakeArrayPtr(expected));
    }

    {
      ezFileChangedEvent expected[] = {
        ezFileChangedEvent(sFilePath, {}, ezFileChangedEvent::Type::FileRemoved)};
      CompareFiles(ezMakeArrayPtr(expected));
    }

    {
      EZ_LOCK(fileEventLock);
      EZ_LOCK(folderEventLock);
      // Check file removed before folder.
      EZ_TEST_BOOL(fileEventTimestamps[0] < folderEventTimestamps[0]);
    }

    ClearFolders();
    ClearFiles();

    EZ_TEST_INT(ezFileSystemModel::GetSingleton()->GetFiles()->GetCount(), 0);
    EZ_TEST_INT(ezFileSystemModel::GetSingleton()->GetFolders()->GetCount(), 1);
  }

  EZ_TEST_BLOCK(ezTestBlock::Enabled, "Shutdown2")
  {
    ezFileSystemModel::GetSingleton()->Deinitialize();
    ezFileChangedEvent expected[] = {ezFileChangedEvent({}, {}, ezFileChangedEvent::Type::ModelReset)};
    CompareFiles(ezMakeArrayPtr(expected));
    ClearFiles();

    ezFolderChangedEvent expected2[] = {ezFolderChangedEvent({}, ezFolderChangedEvent::Type::ModelReset)};
    CompareFolders(ezMakeArrayPtr(expected2));
    ClearFolders();

    ezFileSystemModel::GetSingleton()->m_FileChangedEvents.RemoveEventHandler(fileId);
    ezFileSystemModel::GetSingleton()->m_FolderChangedEvents.RemoveEventHandler(folderId);
  }
}

#endif
