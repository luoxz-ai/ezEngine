#include <QBoxLayout>
#include <QDebug>
#include <QMenu>
#include <QMouseEvent>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QStyle>
#include <QToolButton>

#include "DockAreaTabBar.h"
#include "DockAreaWidget.h"
#include "DockManager.h"
#include "DockOverlay.h"
#include "DockWidget.h"
#include "DockWidgetTab.h"
#include "FloatingDockContainer.h"
#include "FloatingDragPreview.h"
#include "IconProvider.h"
#include "ads_globals.h"

#include <iostream>

namespace ads
{
using tTitleBarButton = QToolButton;


/**
 * Private data class of CDockAreaTitleBar class (pimpl)
 */
struct DockAreaTitleBarPrivate
{
	CDockAreaTitleBar* _this;
	QPointer<tTitleBarButton> TabsMenuButton;
	QPointer<tTitleBarButton> UndockButton;
	QPointer<tTitleBarButton> CloseButton;
	QBoxLayout* Layout;
	CDockAreaWidget* DockArea;
	CDockAreaTabBar* TabBar;
	bool MenuOutdated = true;
	QMenu* TabsMenu;
	QList<tTitleBarButton*> DockWidgetActionsButtons;

	QPoint DragStartMousePos;
	eDragState DragState = DraggingInactive;
	IFloatingWidget* FloatingWidget = nullptr;


	/**
	 * Private data constructor
	 */
	DockAreaTitleBarPrivate(CDockAreaTitleBar* _public);

	/**
	 * Creates the title bar close and menu buttons
	 */
	void createButtons();

	/**
	 * Creates the internal TabBar
	 */
	void createTabBar();

	/**
	 * Convenience function for DockManager access
	 */
	CDockManager* dockManager() const
	{
		return DockArea->dockManager();
	}

	/**
	 * Returns true if the given config flag is set
	 */
	static bool testConfigFlag(CDockManager::eConfigFlag Flag)
	{
		return CDockManager::configFlags().testFlag(Flag);
	}

	/**
	 * Test function for current drag state
	 */
	bool isDraggingState(eDragState dragState) const
	{
		return this->DragState == dragState;
	}


	/**
	 * Starts floating
	 */
	void startFloating(const QPoint& Offset);

	/**
	 * Makes the dock area floating
	 */
	IFloatingWidget* makeAreaFloating(const QPoint& Offset, eDragState DragState);
};// struct DockAreaTitleBarPrivate


/**
 * Title bar button of a dock area that customizes tTitleBarButton appearance/behaviour
 * according to various config flags such as:
 * CDockManager::DockAreaHas_xxx_Button - if set to 'false' keeps the button always invisible
 * CDockManager::DockAreaHideDisabledButtons - if set to 'true' hides button when it is disabled
 */
class CTitleBarButton : public tTitleBarButton
{
	Q_OBJECT
	bool Visible = true;
	bool HideWhenDisabled = false;
public:
	using Super = tTitleBarButton;
	CTitleBarButton(bool visible = true, QWidget* parent = nullptr)
		: tTitleBarButton(parent),
		  Visible(visible),
		  HideWhenDisabled(DockAreaTitleBarPrivate::testConfigFlag(CDockManager::DockAreaHideDisabledButtons))
	{}
	

	/**
	 * Adjust this visibility change request with our internal settings:
	 */
	virtual void setVisible(bool visible) override
	{
		// 'visible' can stay 'true' if and only if this button is configured to generaly visible:
		visible = visible && this->Visible;

		// 'visible' can stay 'true' unless: this button is configured to be invisible when it is disabled and it is currently disabled:
		if(visible && HideWhenDisabled)
		{
			visible = isEnabled();
		}

		Super::setVisible(visible);
	}
	
protected:
	/**
	 * Handle EnabledChanged signal to set button invisible if the configured
	 */
	bool event(QEvent *ev) override
	{
		if(QEvent::EnabledChange == ev->type() && HideWhenDisabled)
		{
			// force setVisible() call 
			// Calling setVisible() directly here doesn't work well when button is expected to be shown first time
			QMetaObject::invokeMethod(this, "setVisible", Qt::QueuedConnection, Q_ARG(bool, isEnabled()));
		}

		return Super::event(ev);
	}
};


/**
 * This spacer widget is here because of the following problem.
 * The dock area title bar handles mouse dragging and moving the floating widget.
 * The  problem is, that if the title bar becomes invisible, i.e. if the dock
 * area contains only one single dock widget and the dock area is moved
 * into a floating widget, then mouse events are not handled anymore and dragging
 * of the floating widget stops.
 */
class CSpacerWidget : public QWidget
{
	Q_OBJECT
public:
	using Super = QWidget;
	CSpacerWidget(QWidget* Parent = 0)
	    : Super(Parent)
	{
	    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	    setStyleSheet("border: none; background: none;");
	}
	virtual QSize sizeHint() const override {return QSize(0, 0);}
	virtual QSize minimumSizeHint() const override {return QSize(0, 0);}
};


} // namespace ads
