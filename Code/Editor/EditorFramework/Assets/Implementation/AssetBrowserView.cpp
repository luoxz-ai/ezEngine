#include <EditorFramework/EditorFrameworkPCH.h>

#include <EditorFramework/Assets/AssetBrowserModel.moc.h>
#include <EditorFramework/Assets/AssetBrowserView.moc.h>
#include <EditorFramework/Assets/AssetCurator.h>
#include <GuiFoundation/UIServices/UIServices.moc.h>


ezQtAssetBrowserView::ezQtAssetBrowserView(QWidget* pParent)
  : ezQtItemView<QListView>(pParent)
{
  m_iIconSizePercentage = 100;
  m_pDelegate = new ezQtIconViewDelegate(this);

  SetDialogMode(false);

  setSelectionBehavior(QAbstractItemView::SelectionBehavior::SelectItems);
  setViewMode(QListView::ViewMode::IconMode);
  setUniformItemSizes(true);
  setResizeMode(QListView::ResizeMode::Adjust);

  setItemDelegate(m_pDelegate);
  SetIconScale(m_iIconSizePercentage);
}

void ezQtAssetBrowserView::SetDialogMode(bool bDialogMode)
{
  m_bDialogMode = bDialogMode;

  if (m_bDialogMode)
  {
    m_pDelegate->SetDrawTransformState(false);
    setDragDropMode(QAbstractItemView::DragDropMode::NoDragDrop);
    setSelectionMode(QAbstractItemView::SelectionMode::SingleSelection);
  }
  else
  {
    m_pDelegate->SetDrawTransformState(true);
    setDragDropMode(QAbstractItemView::DragOnly);
    setSelectionMode(QAbstractItemView::SelectionMode::ExtendedSelection);
  }
}

void ezQtAssetBrowserView::SetIconMode(bool bIconMode)
{
  if (bIconMode)
  {
    setViewMode(QListView::ViewMode::IconMode);
    SetIconScale(m_iIconSizePercentage);
  }
  else
  {
    setViewMode(QListView::ViewMode::ListMode);
    setGridSize(QSize());
  }
}

void ezQtAssetBrowserView::SetIconScale(ezInt32 iIconSizePercentage)
{
  m_iIconSizePercentage = ezMath::Clamp(iIconSizePercentage, 10, 100);
  m_pDelegate->SetIconScale(m_iIconSizePercentage);

  if (viewMode() != QListView::ViewMode::IconMode)
    return;

  setGridSize(m_pDelegate->sizeHint(QStyleOptionViewItem(), QModelIndex()));
}

ezInt32 ezQtAssetBrowserView::GetIconScale() const
{
  return m_iIconSizePercentage;
}


void ezQtAssetBrowserView::wheelEvent(QWheelEvent* pEvent)
{
  if (pEvent->modifiers() == Qt::CTRL)
  {
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    ezInt32 iDelta = pEvent->angleDelta().y() > 0 ? 5 : -5;
#else
    ezInt32 iDelta = pEvent->delta() > 0 ? 5 : -5;
#endif
    SetIconScale(m_iIconSizePercentage + iDelta);
    Q_EMIT ViewZoomed(m_iIconSizePercentage);
    return;
  }

  QListView::wheelEvent(pEvent);
}

ezQtIconViewDelegate::ezQtIconViewDelegate(ezQtAssetBrowserView* pParent)
  : ezQtItemDelegate(pParent)
{
  m_bDrawTransformState = true;
  m_iIconSizePercentage = 100;
  m_pView = pParent;
}

void ezQtIconViewDelegate::SetIconScale(ezInt32 iIconSizePercentage)
{
  m_iIconSizePercentage = iIconSizePercentage;
}

bool ezQtIconViewDelegate::mousePressEvent(QMouseEvent* pEvent, const QStyleOptionViewItem& opt, const QModelIndex& index)
{
  const ezUInt32 uiThumbnailSize = ThumbnailSize();
  QRect thumbnailRect = opt.rect.adjusted(ItemSideMargin + uiThumbnailSize - 16 + 2, ItemSideMargin + uiThumbnailSize - 16 + 2, 0, 0);
  thumbnailRect.setSize(QSize(16, 16));
  if (thumbnailRect.contains(pEvent->localPos().toPoint()))
  {
    pEvent->accept();
    return true;
  }
  return false;
}

bool ezQtIconViewDelegate::mouseReleaseEvent(QMouseEvent* pEvent, const QStyleOptionViewItem& opt, const QModelIndex& index)
{
  const ezUInt32 uiThumbnailSize = ThumbnailSize();
  QRect thumbnailRect = opt.rect.adjusted(ItemSideMargin + uiThumbnailSize - 16 + 2, ItemSideMargin + uiThumbnailSize - 16 + 2, 0, 0);
  thumbnailRect.setSize(QSize(16, 16));
  if (thumbnailRect.contains(pEvent->localPos().toPoint()))
  {
    ezUuid guid = index.data(ezQtAssetBrowserModel::UserRoles::AssetGuid).value<ezUuid>();

    ezTransformStatus ret = ezAssetCurator::GetSingleton()->TransformAsset(guid, ezTransformFlags::TriggeredManually);

    if (ret.Failed())
    {
      QString path = index.data(ezQtAssetBrowserModel::UserRoles::RelativePath).toString();
      ezLog::Error("Transform failed: '{0}' ({1})", ret.m_sMessage, path.toUtf8().data());
    }
    else
    {
      ezAssetCurator::GetSingleton()->WriteAssetTables().IgnoreResult();
    }

    pEvent->accept();
    return true;
  }
  return false;
}

void ezQtIconViewDelegate::paint(QPainter* pPainter, const QStyleOptionViewItem& opt, const QModelIndex& index) const
{
  if (!IsInIconMode())
  {
    ezQtItemDelegate::paint(pPainter, opt, index);
    return;
  }

  const ezUInt32 uiThumbnailSize = ThumbnailSize();

  // Prepare painter.
  {
    pPainter->save();
    if (hasClipping())
      pPainter->setClipRect(opt.rect);

    pPainter->setRenderHint(QPainter::SmoothPixmapTransform, true);
  }

  // Draw highlight background (copy of QItemDelegate::drawBackground)
  {
    QRect highlightRect = opt.rect.adjusted(ItemSideMargin - HighlightBorderWidth, ItemSideMargin - HighlightBorderWidth, 0, 0);
    highlightRect.setHeight(uiThumbnailSize + 2 * HighlightBorderWidth);
    highlightRect.setWidth(uiThumbnailSize + 2 * HighlightBorderWidth);

    if ((opt.state & QStyle::State_Selected))
    {
      QPalette::ColorGroup cg = opt.state & QStyle::State_Enabled ? QPalette::Normal : QPalette::Disabled;
      if (cg == QPalette::Normal && !(opt.state & QStyle::State_Active))
        cg = QPalette::Inactive;

      pPainter->fillRect(highlightRect, opt.palette.brush(cg, QPalette::Highlight));
    }
    else
    {
      QVariant value = index.data(Qt::BackgroundRole);
      if (value.canConvert<QBrush>())
      {
        QPointF oldBO = pPainter->brushOrigin();
        pPainter->setBrushOrigin(highlightRect.topLeft());
        pPainter->fillRect(highlightRect, qvariant_cast<QBrush>(value));
        pPainter->setBrushOrigin(oldBO);
      }
    }
  }

  // Draw thumbnail.
  {
    QRect thumbnailRect = opt.rect.adjusted(ItemSideMargin, ItemSideMargin, 0, 0);
    thumbnailRect.setSize(QSize(uiThumbnailSize, uiThumbnailSize));
    QPixmap pixmap = qvariant_cast<QPixmap>(index.data(Qt::DecorationRole));
    pPainter->drawPixmap(thumbnailRect, pixmap);
  }

  // Draw icon.
  {
    QRect thumbnailRect = opt.rect.adjusted(ItemSideMargin - 2, ItemSideMargin + uiThumbnailSize - 16 + 2, 0, 0);
    thumbnailRect.setSize(QSize(16, 16));
    QIcon icon = qvariant_cast<QIcon>(index.data(ezQtAssetBrowserModel::UserRoles::AssetIconPath));
    icon.paint(pPainter, thumbnailRect);
  }

  // Draw Transform State Icon
  if (m_bDrawTransformState)
  {
    QRect thumbnailRect = opt.rect.adjusted(ItemSideMargin + uiThumbnailSize - 16 + 2, ItemSideMargin + uiThumbnailSize - 16 + 2, 0, 0);
    thumbnailRect.setSize(QSize(16, 16));

    ezAssetInfo::TransformState state = (ezAssetInfo::TransformState)index.data(ezQtAssetBrowserModel::UserRoles::TransformState).toInt();

    switch (state)
    {
      case ezAssetInfo::TransformState::Unknown:
        ezQtUiServices::GetSingleton()->GetCachedIconResource(":/EditorFramework/Icons/AssetUnknown16.png").paint(pPainter, thumbnailRect);
        break;
      case ezAssetInfo::TransformState::NeedsThumbnail:
        ezQtUiServices::GetSingleton()->GetCachedIconResource(":/EditorFramework/Icons/AssetNeedsThumbnail16.png").paint(pPainter, thumbnailRect);
        break;
      case ezAssetInfo::TransformState::NeedsTransform:
        ezQtUiServices::GetSingleton()->GetCachedIconResource(":/EditorFramework/Icons/AssetNeedsTransform16.png").paint(pPainter, thumbnailRect);
        break;
      case ezAssetInfo::TransformState::UpToDate:
        ezQtUiServices::GetSingleton()->GetCachedIconResource(":/EditorFramework/Icons/AssetOk16.png").paint(pPainter, thumbnailRect);
        break;
      case ezAssetInfo::TransformState::MissingTransformDependency:
        ezQtUiServices::GetSingleton()->GetCachedIconResource(":/EditorFramework/Icons/AssetMissingDependency16.png").paint(pPainter, thumbnailRect);
        break;
      case ezAssetInfo::TransformState::MissingThumbnailDependency:
        ezQtUiServices::GetSingleton()->GetCachedIconResource(":/EditorFramework/Icons/AssetMissingReference16.png").paint(pPainter, thumbnailRect);
        break;
      case ezAssetInfo::TransformState::CircularDependency:
        ezQtUiServices::GetSingleton()->GetCachedIconResource(":/EditorFramework/Icons/AssetFailedTransform16.png").paint(pPainter, thumbnailRect);
        break;
      case ezAssetInfo::TransformState::TransformError:
        ezQtUiServices::GetSingleton()->GetCachedIconResource(":/EditorFramework/Icons/AssetFailedTransform16.png").paint(pPainter, thumbnailRect);
        break;
      case ezAssetInfo::TransformState::NeedsImport:
        ezQtUiServices::GetSingleton()->GetCachedIconResource(":/EditorFramework/Icons/AssetNeedsImport16.png").paint(pPainter, thumbnailRect);
        break;
      case ezAssetInfo::TransformState::COUNT:
        break;
    }
  }

  // Draw caption.
  {
    pPainter->setFont(GetFont());
    QRect textRect = opt.rect.adjusted(ItemSideMargin, ItemSideMargin + uiThumbnailSize + TextSpacing, -ItemSideMargin, -ItemSideMargin - TextSpacing);

    QString caption = qvariant_cast<QString>(index.data(Qt::DisplayRole));
    pPainter->drawText(textRect, Qt::AlignHCenter | Qt::AlignTop | Qt::TextWrapAnywhere, caption);
  }


  pPainter->restore();
}

QSize ezQtIconViewDelegate::sizeHint(const QStyleOptionViewItem& opt, const QModelIndex& index) const
{
  if (IsInIconMode())
  {
    return ItemSize();
  }
  else
  {
    return ezQtItemDelegate::sizeHint(opt, index);
  }
}

QSize ezQtIconViewDelegate::ItemSize() const
{
  QFont font = GetFont();
  QFontMetrics fm(font);

  ezUInt32 iThumbnail = ThumbnailSize();
  const ezUInt32 iItemWidth = iThumbnail + 2 * ItemSideMargin;
  const ezUInt32 iItemHeight = iThumbnail + 2 * (ItemSideMargin + fm.height() + TextSpacing);

  return QSize(iItemWidth, iItemHeight);
}

QFont ezQtIconViewDelegate::GetFont() const
{
  QFont font = QApplication::font();

  float fScaleFactor = ezMath::Clamp((1.0f + (m_iIconSizePercentage / 100.0f)) * 0.75f, 0.75f, 1.25f);

  font.setPointSizeF(font.pointSizeF() * fScaleFactor);
  return font;
}

ezUInt32 ezQtIconViewDelegate::ThumbnailSize() const
{
  return MaxSize * (float)m_iIconSizePercentage / 100.0f;
}

bool ezQtIconViewDelegate::IsInIconMode() const
{
  return m_pView->viewMode() == QListView::ViewMode::IconMode;
}
