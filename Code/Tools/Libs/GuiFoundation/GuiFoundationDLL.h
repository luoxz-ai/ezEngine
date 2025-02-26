#pragma once

#include <Foundation/Basics.h>
#include <Foundation/Math/Color8UNorm.h>
#include <Foundation/Types/Uuid.h>
#include <QColor>
#include <QMetaType>
#include <ToolsFoundation/ToolsFoundationDLL.h>

// Configure the DLL Import/Export Define
#if EZ_ENABLED(EZ_COMPILE_ENGINE_AS_DLL)
#  ifdef BUILDSYSTEM_BUILDING_GUIFOUNDATION_LIB
#    define EZ_GUIFOUNDATION_DLL EZ_DECL_EXPORT
#  else
#    define EZ_GUIFOUNDATION_DLL EZ_DECL_IMPORT
#  endif
#else
#  define EZ_GUIFOUNDATION_DLL
#endif

class QWidget;
class QObject;


Q_DECLARE_METATYPE(ezUuid);

/// \brief Calls setUpdatesEnabled(false) on all given QObjects, and the reverse in the destructor. Can be nested.
class EZ_GUIFOUNDATION_DLL ezQtScopedUpdatesDisabled
{
public:
  ezQtScopedUpdatesDisabled(QWidget* pWidget1, QWidget* pWidget2 = nullptr, QWidget* pWidget3 = nullptr, QWidget* pWidget4 = nullptr,
    QWidget* pWidget5 = nullptr, QWidget* pWidget6 = nullptr);
  ~ezQtScopedUpdatesDisabled();

private:
  QWidget* m_pWidgets[6];
};


/// \brief Calls blockSignals(true) on all given QObjects, and the reverse in the destructor. Can be nested.
class EZ_GUIFOUNDATION_DLL ezQtScopedBlockSignals
{
public:
  ezQtScopedBlockSignals(QObject* pObject1, QObject* pObject2 = nullptr, QObject* pObject3 = nullptr, QObject* pObject4 = nullptr,
    QObject* pObject5 = nullptr, QObject* pObject6 = nullptr);
  ~ezQtScopedBlockSignals();

private:
  QObject* m_pObjects[6];
};

EZ_ALWAYS_INLINE QColor ezToQtColor(const ezColorGammaUB& c)
{
  return QColor(c.r, c.g, c.b, c.a);
}

EZ_ALWAYS_INLINE ezColorGammaUB qtToEzColor(const QColor& c)
{
  return ezColorGammaUB(c.red(), c.green(), c.blue(), c.alpha());
}
