#include <GuiFoundation/GuiFoundationPCH.h>

#include <Foundation/Configuration/CVar.h>
#include <GuiFoundation/GuiFoundationDLL.h>
#include <GuiFoundation/UIServices/UIServices.moc.h>
#include <GuiFoundation/Widgets/CVarWidget.moc.h>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>

ezQtCVarWidget::ezQtCVarWidget(QWidget* pParent)
  : QWidget(pParent)
{
  setupUi(this);

  m_pItemModel = new ezQtCVarModel(this);

  m_pFilterModel = new QSortFilterProxyModel(this);
  m_pFilterModel->setSourceModel(m_pItemModel);

  m_pItemDelegate = new ezQtCVarItemDelegate(this);
  m_pItemDelegate->m_pModel = m_pItemModel;

  CVarsView->setModel(m_pFilterModel);
  CVarsView->setSelectionBehavior(QAbstractItemView::SelectionBehavior::SelectRows);
  CVarsView->setSelectionMode(QAbstractItemView::SelectionMode::SingleSelection);
  CVarsView->setHeaderHidden(false);
  CVarsView->setEditTriggers(QAbstractItemView::EditTrigger::CurrentChanged | QAbstractItemView::EditTrigger::SelectedClicked);
  CVarsView->setItemDelegateForColumn(1, m_pItemDelegate);

  connect(SearchWidget, &ezQtSearchWidget::textChanged, this, &ezQtCVarWidget::SearchTextChanged);
  connect(ConsoleInput, &ezQtSearchWidget::enterPressed, this, &ezQtCVarWidget::ConsoleEnterPressed);
  connect(ConsoleInput, &ezQtSearchWidget::specialKeyPressed, this, &ezQtCVarWidget::ConsoleSpecialKeyPressed);

  m_Console.Events().AddEventHandler(ezMakeDelegate(&ezQtCVarWidget::OnConsoleEvent, this));

  ConsoleInput->setPlaceholderText("> TAB to auto-complete");
}

ezQtCVarWidget::~ezQtCVarWidget() = default;

void ezQtCVarWidget::Clear()
{
  QPointer<QWidget> pFocusWidget = QApplication::focusWidget();
  clearFocus();
  m_pItemModel->BeginResetModel();
  m_pItemModel->EndResetModel();

  if (pFocusWidget)
    pFocusWidget->setFocus();
}

void ezQtCVarWidget::RebuildCVarUI(const ezMap<ezString, ezCVarWidgetData>& cvars)
{
  // for now update and rebuild are the same
  UpdateCVarUI(cvars);
}

void ezQtCVarWidget::UpdateCVarUI(const ezMap<ezString, ezCVarWidgetData>& cvars)
{
  int row = 0;

  m_pItemModel->BeginResetModel();

  for (auto it = cvars.GetIterator(); it.IsValid(); ++it, ++row)
  {
    it.Value().m_bNewEntry = false;

    auto item = m_pItemModel->CreateEntry(it.Key().GetData());
    item->m_sDescription = it.Value().m_sDescription;
    item->m_sPlugin = it.Value().m_sPlugin;

    switch (it.Value().m_uiType)
    {
      case ezCVarType::Bool:
        item->m_Value = it.Value().m_bValue;
        break;
      case ezCVarType::Float:
        item->m_Value = it.Value().m_fValue;
        break;
      case ezCVarType::Int:
        item->m_Value = it.Value().m_iValue;
        break;
      case ezCVarType::String:
        item->m_Value = it.Value().m_sValue;
        break;
    }
  }

  m_pItemModel->EndResetModel();

  CVarsView->expandAll();
  CVarsView->resizeColumnToContents(0);
  CVarsView->resizeColumnToContents(1);
}

void ezQtCVarWidget::AddConsoleStrings(const ezStringBuilder& sEncoded)
{
  ezHybridArray<ezStringView, 64> lines;
  sEncoded.Split(false, lines, ";;");

  ezStringBuilder tmp;

  for (auto l : lines)
  {
    l.Shrink(4, 0); // skip the line type number at the front (for now)

    if (l.StartsWith("<"))
    {
      l.Shrink(1, 0);
      ConsoleInput->setText(l.GetData(tmp));
    }
    else
    {
      GetConsole().AddConsoleString(l);
    }
  }
}

void ezQtCVarWidget::SearchTextChanged(const QString& text)
{
  m_pFilterModel->setRecursiveFilteringEnabled(true);
  m_pFilterModel->setFilterRole(Qt::UserRole);
  m_pFilterModel->setFilterCaseSensitivity(Qt::CaseSensitivity::CaseInsensitive);

  QRegularExpression e;

  QString st = "(?=.*" + text + ".*)";
  st.replace(" ", ".*)(?=.*");

  e.setPattern(st);
  e.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
  m_pFilterModel->setFilterRegularExpression(e);
  CVarsView->expandAll();
}

void ezQtCVarWidget::ConsoleEnterPressed()
{
  m_Console.AddToInputHistory(ConsoleInput->text().toUtf8().data());
  m_Console.ExecuteCommand(ConsoleInput->text().toUtf8().data());
  ConsoleInput->setText("");
}

void ezQtCVarWidget::ConsoleSpecialKeyPressed(Qt::Key key)
{
  if (key == Qt::Key_Tab)
  {
    ezStringBuilder input = ConsoleInput->text().toUtf8().data();

    if (m_Console.AutoComplete(input))
    {
      ConsoleInput->setText(input.GetData());
    }
  }
  if (key == Qt::Key_Up)
  {
    ezStringBuilder input = ConsoleInput->text().toUtf8().data();
    m_Console.RetrieveInputHistory(1, input);
    ConsoleInput->setText(input.GetData());
  }
  if (key == Qt::Key_Down)
  {
    ezStringBuilder input = ConsoleInput->text().toUtf8().data();
    m_Console.RetrieveInputHistory(-1, input);
    ConsoleInput->setText(input.GetData());
  }
  if (key == Qt::Key_F2)
  {
    if (m_Console.GetInputHistory().GetCount() >= 1)
    {
      m_Console.ExecuteCommand(m_Console.GetInputHistory()[0]);
    }
  }
  if (key == Qt::Key_F3)
  {
    if (m_Console.GetInputHistory().GetCount() >= 2)
    {
      m_Console.ExecuteCommand(m_Console.GetInputHistory()[1]);
    }
  }
}

void ezQtCVarWidget::OnConsoleEvent(const ezConsoleEvent& e)
{
  if (e.m_Type == ezConsoleEvent::Type::OutputLineAdded)
  {
    QString t = ConsoleOutput->toPlainText();
    t += e.m_AddedpConsoleString->m_sText;
    t += "\n";
    ConsoleOutput->setPlainText(t);
    ConsoleOutput->verticalScrollBar()->setValue(ConsoleOutput->verticalScrollBar()->maximum());
  }
}

ezQtCVarModel::ezQtCVarModel(ezQtCVarWidget* pOwner)
  : QAbstractItemModel(pOwner)
{
  m_pOwner = pOwner;
}

ezQtCVarModel::~ezQtCVarModel() = default;

void ezQtCVarModel::BeginResetModel()
{
  beginResetModel();
  m_RootEntries.Clear();
  m_AllEntries.Clear();
}

void ezQtCVarModel::EndResetModel()
{
  endResetModel();
}

QVariant ezQtCVarModel::headerData(int iSection, Qt::Orientation orientation, int iRole /*= Qt::DisplayRole*/) const
{
  if (iRole == Qt::DisplayRole)
  {
    switch (iSection)
    {
      case 0:
        return "Name";

      case 1:
        return "Value";

      case 2:
        return "Description";

      case 3:
        return "Plugin";
    }
  }

  return QAbstractItemModel::headerData(iSection, orientation, iRole);
}

bool ezQtCVarModel::setData(const QModelIndex& index, const QVariant& value, int iRole /*= Qt::EditRole*/)
{
  if (index.column() == 1 && iRole == Qt::EditRole)
  {
    ezQtCVarModel::Entry* e = reinterpret_cast<ezQtCVarModel::Entry*>(index.internalId());

    switch (e->m_Value.GetType())
    {
      case ezVariantType::Bool:
        e->m_Value = value.toBool();
        m_pOwner->onBoolChanged(e->m_sFullName, value.toBool());
        break;
      case ezVariantType::Int32:
        e->m_Value = value.toInt();
        m_pOwner->onIntChanged(e->m_sFullName, value.toInt());
        break;
      case ezVariantType::Float:
        e->m_Value = value.toFloat();
        m_pOwner->onFloatChanged(e->m_sFullName, value.toFloat());
        break;
      case ezVariantType::String:
        e->m_Value = value.toString().toUtf8().data();
        m_pOwner->onStringChanged(e->m_sFullName, value.toString().toUtf8().data());
        break;
      default:
        break;
    }
  }

  return QAbstractItemModel::setData(index, value, iRole);
}

QVariant ezQtCVarModel::data(const QModelIndex& index, int iRole) const
{
  if (!index.isValid())
    return QVariant();

  ezQtCVarModel::Entry* e = reinterpret_cast<ezQtCVarModel::Entry*>(index.internalId());

  if (iRole == Qt::UserRole)
  {
    return e->m_sFullName.GetData();
  }

  if (iRole == Qt::DisplayRole)
  {
    switch (index.column())
    {
      case 0:
        return e->m_sDisplayString;

      case 1:
        return e->m_Value.ConvertTo<ezString>().GetData();

      case 2:
        return e->m_sDescription;
    }
  }

  if (iRole == Qt::DecorationRole && index.column() == 0)
  {
    if (e->m_Value.IsValid())
    {
      return ezQtUiServices::GetCachedIconResource(":/GuiFoundation/Icons/CVar.png");
    }
  }

  if (iRole == Qt::ToolTipRole)
  {
    if (e->m_Value.IsValid())
    {
      if (index.column() == 0)
      {
        return QString(e->m_sFullName) + " | " + e->m_sPlugin;
      }

      if (index.column() == 2)
      {
        return e->m_sDescription;
      }
    }
  }

  if (iRole == Qt::EditRole && index.column() == 1)
  {
    switch (e->m_Value.GetType())
    {
      case ezVariantType::Bool:
        return e->m_Value.Get<bool>();
      case ezVariantType::Int32:
        return e->m_Value.Get<ezInt32>();
      case ezVariantType::Float:
        return e->m_Value.ConvertTo<double>();
      case ezVariantType::String:
        return e->m_Value.Get<ezString>().GetData();
      default:
        break;
    }
  }
  return QVariant();
}

Qt::ItemFlags ezQtCVarModel::flags(const QModelIndex& index) const
{
  if (index.column() == 1)
  {
    ezQtCVarModel::Entry* e = reinterpret_cast<ezQtCVarModel::Entry*>(index.internalId());

    if (e->m_Value.IsValid())
    {
      return Qt::ItemFlag::ItemIsSelectable | Qt::ItemFlag::ItemIsEnabled | Qt::ItemFlag::ItemIsEditable;
    }
  }

  return Qt::ItemFlag::ItemIsSelectable | Qt::ItemFlag::ItemIsEnabled;
}

QModelIndex ezQtCVarModel::index(int iRow, int iColumn, const QModelIndex& parent /*= QModelIndex()*/) const
{
  if (parent.isValid())
  {
    ezQtCVarModel::Entry* e = reinterpret_cast<ezQtCVarModel::Entry*>(parent.internalId());
    return createIndex(iRow, iColumn, const_cast<ezQtCVarModel::Entry*>(e->m_ChildEntries[iRow]));
  }
  else
  {
    return createIndex(iRow, iColumn, const_cast<ezQtCVarModel::Entry*>(m_RootEntries[iRow]));
  }
}

QModelIndex ezQtCVarModel::parent(const QModelIndex& index) const
{
  if (!index.isValid())
    return QModelIndex();

  ezQtCVarModel::Entry* e = reinterpret_cast<ezQtCVarModel::Entry*>(index.internalId());

  if (e->m_pParentEntry == nullptr)
    return QModelIndex();

  ezQtCVarModel::Entry* p = e->m_pParentEntry;

  // find the parent entry's row index
  if (p->m_pParentEntry == nullptr)
  {
    // if the parent has no parent itself, it is a root entry and we need to search that array
    for (ezUInt32 row = 0; row < m_RootEntries.GetCount(); ++row)
    {
      if (m_RootEntries[row] == p)
      {
        return createIndex(row, index.column(), p);
      }
    }
  }
  else
  {
    // if the parent has a parent itself, search that array for the row index
    for (ezUInt32 row = 0; row < p->m_pParentEntry->m_ChildEntries.GetCount(); ++row)
    {
      if (p->m_pParentEntry->m_ChildEntries[row] == e)
      {
        return createIndex(row, index.column(), p);
      }
    }
  }

  return QModelIndex();
}

int ezQtCVarModel::rowCount(const QModelIndex& parent /*= QModelIndex()*/) const
{
  if (parent.isValid())
  {
    ezQtCVarModel::Entry* e = reinterpret_cast<ezQtCVarModel::Entry*>(parent.internalId());

    return (int)e->m_ChildEntries.GetCount();
  }
  else
  {
    return (int)m_RootEntries.GetCount();
  }
}

int ezQtCVarModel::columnCount(const QModelIndex& index /*= QModelIndex()*/) const
{
  return 3;
}

ezQtCVarModel::Entry* ezQtCVarModel::CreateEntry(const char* szName)
{
  ezStringBuilder tmp = szName;
  ezStringBuilder tmp2;

  ezHybridArray<ezStringView, 8> pieces;
  tmp.Split(false, pieces, ".", "_");

  ezDynamicArray<Entry*>* vals = &m_RootEntries;
  Entry* parentEntry = nullptr;

  for (ezUInt32 p = 0; p < pieces.GetCount(); ++p)
  {
    QString piece = pieces[p].GetData(tmp2);
    for (ezUInt32 v = 0; v < vals->GetCount(); ++v)
    {
      if ((*vals)[v]->m_sDisplayString == piece)
      {
        parentEntry = (*vals)[v];
        vals = &((*vals)[v]->m_ChildEntries);
        goto found;
      }
    }

    {
      auto& newItem = m_AllEntries.ExpandAndGetRef();
      newItem.m_sFullName = szName;
      newItem.m_sDisplayString = piece;
      newItem.m_pParentEntry = parentEntry;

      vals->PushBack(&newItem);

      parentEntry = &newItem;
      vals = &newItem.m_ChildEntries;
    }
  found:;
  }

  return parentEntry;
}

QWidget* ezQtCVarItemDelegate::createEditor(QWidget* pParent, const QStyleOptionViewItem& option, const QModelIndex& idx) const
{
  m_Index = static_cast<const QSortFilterProxyModel*>(idx.model())->mapToSource(idx);
  ezQtCVarModel::Entry* e = reinterpret_cast<ezQtCVarModel::Entry*>(m_Index.internalPointer());

  if (!e->m_Value.IsValid())
    return nullptr;

  if (e->m_Value.IsA<bool>())
  {
    QComboBox* ret = new QComboBox(pParent);
    ret->addItem("true");
    ret->addItem("false");

    connect(ret, SIGNAL(currentIndexChanged(int)), this, SLOT(onComboChanged(int)));
    return ret;
  }

  if (e->m_Value.IsA<ezInt32>())
  {
    QLineEdit* ret = new QLineEdit(pParent);
    ret->setValidator(new QIntValidator(ret));
    return ret;
  }

  if (e->m_Value.IsA<float>())
  {
    QLineEdit* ret = new QLineEdit(pParent);
    auto val = new QDoubleValidator(ret);
    val->setDecimals(3);
    ret->setValidator(val);
    return ret;
  }

  if (e->m_Value.IsA<ezString>())
  {
    QLineEdit* ret = new QLineEdit(pParent);
    return ret;
  }

  return nullptr;
}

void ezQtCVarItemDelegate::setEditorData(QWidget* pEditor, const QModelIndex& index) const
{
  QVariant value = index.model()->data(index, Qt::EditRole);

  if (QLineEdit* pLine = qobject_cast<QLineEdit*>(pEditor))
  {
    if (value.typeId() == QMetaType::Double)
    {
      double f = value.toDouble();

      pLine->setText(QString("%1").arg(f, 0, (char)103, 3));
    }
    else
    {
      pLine->setText(value.toString());
    }

    pLine->selectAll();
  }

  if (QComboBox* pLine = qobject_cast<QComboBox*>(pEditor))
  {
    pLine->setCurrentIndex(value.toBool() ? 0 : 1);
    pLine->showPopup();
  }
}

void ezQtCVarItemDelegate::setModelData(QWidget* pEditor, QAbstractItemModel* pModel, const QModelIndex& index) const
{
  if (QLineEdit* pLine = qobject_cast<QLineEdit*>(pEditor))
  {
    pModel->setData(index, pLine->text(), Qt::EditRole);
  }

  if (QComboBox* pLine = qobject_cast<QComboBox*>(pEditor))
  {
    pModel->setData(index, pLine->currentText(), Qt::EditRole);
  }
}

void ezQtCVarItemDelegate::onComboChanged(int)
{
  setModelData(qobject_cast<QWidget*>(sender()), m_pModel, m_Index);
}
