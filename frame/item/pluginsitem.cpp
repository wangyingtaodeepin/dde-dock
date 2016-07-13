#include "pluginsitem.h"
#include "pluginsiteminterface.h"

#include "util/imagefactory.h"

#include <QPainter>
#include <QBoxLayout>
#include <QMouseEvent>
#include <QDrag>
#include <QMimeData>

#define PLUGIN_ITEM_DRAG_THRESHOLD      20

QPoint PluginsItem::MousePressPoint = QPoint();

PluginsItem::PluginsItem(PluginsItemInterface* const pluginInter, const QString &itemKey, QWidget *parent)
    : DockItem(Plugins, parent),
      m_pluginInter(pluginInter),
      m_itemKey(itemKey),
      m_draging(false),
      m_simpleTips(new QLabel(this))
{
    m_pluginType = pluginInter->pluginType(itemKey);

    m_simpleTips->setVisible(false);
    m_simpleTips->setAlignment(Qt::AlignCenter);
    m_simpleTips->setStyleSheet("QLabel {"
                                "color:white;"
                                "padding:5px 10px;"
                                "font-size:14px;"
                                "}");

    if (m_pluginType == PluginsItemInterface::Simple)
        return;

    // construct complex widget layout
    QWidget *centeralWidget = m_pluginInter->itemWidget(itemKey);
    Q_ASSERT(centeralWidget);
    centeralWidget->installEventFilter(this);

    QBoxLayout *hLayout = new QHBoxLayout;
    hLayout->addWidget(centeralWidget);
    hLayout->setSpacing(0);
    hLayout->setMargin(0);

    setLayout(hLayout);
    setAttribute(Qt::WA_TranslucentBackground);
}

PluginsItem::~PluginsItem()
{
}

int PluginsItem::itemSortKey() const
{
    return m_pluginInter->itemSortKey(m_itemKey);
}

void PluginsItem::detachPluginWidget()
{
    if (m_pluginType == PluginsItemInterface::Simple)
        return;

    QWidget *widget = m_pluginInter->itemWidget(m_itemKey);
    if (widget)
        widget->setParent(nullptr);
}

void PluginsItem::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton)
    {
        MousePressPoint = e->pos();
    }
    else if (e->button() == Qt::RightButton)
    {

    }
    else
        DockItem::mousePressEvent(e);
}

void PluginsItem::mouseMoveEvent(QMouseEvent *e)
{
    if (e->buttons() != Qt::LeftButton)
        return DockItem::mouseMoveEvent(e);

    e->accept();

    const QPoint distance = e->pos() - MousePressPoint;
    if (distance.manhattanLength() > PLUGIN_ITEM_DRAG_THRESHOLD)
        startDrag();
}

void PluginsItem::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton)
        return DockItem::mouseReleaseEvent(e);

    e->accept();

    const QPoint distance = e->pos() - MousePressPoint;
    if (distance.manhattanLength() < PLUGIN_ITEM_DRAG_THRESHOLD)
        mouseClicked();
}

void PluginsItem::paintEvent(QPaintEvent *e)
{
    if (m_draging)
        return;

    if (m_pluginType == PluginsItemInterface::Complex)
        return DockItem::paintEvent(e);

    QPainter painter(this);

    const QIcon icon = m_pluginInter->itemIcon(m_itemKey);
    const QRect iconRect = perfectIconRect();
    const QPixmap pixmap = icon.pixmap(iconRect.size());

    painter.drawPixmap(iconRect, pixmap);

    if (m_hover)
        painter.drawPixmap(iconRect, ImageFactory::lighterEffect(pixmap));
}

bool PluginsItem::eventFilter(QObject *o, QEvent *e)
{
    if (m_draging)
        if (o->parent() == this && e->type() == QEvent::Paint)
            return true;

    return DockItem::eventFilter(o, e);
}

QSize PluginsItem::sizeHint() const
{
    if (m_pluginType == PluginsItemInterface::Complex)
        return DockItem::sizeHint();

    // TODO: icon size on efficient mode
    return QSize(24, 24);
}

QWidget *PluginsItem::popupTips()
{
    if (m_pluginInter->tipsType(m_itemKey) == PluginsItemInterface::Complex)
        return m_pluginInter->itemTipsWidget(m_itemKey);

    const QString tips = m_pluginInter->itemTipsString(m_itemKey);
    if (tips.isEmpty())
        return nullptr;

    m_simpleTips->setText(tips);

    return m_simpleTips;
}

void PluginsItem::startDrag()
{
    QPixmap pixmap;
    if (m_pluginType == PluginsItemInterface::Simple)
        pixmap = m_pluginInter->itemIcon(m_itemKey).pixmap(perfectIconRect().size());
    else
        pixmap = grab();

    m_draging = true;
    update();

    QDrag *drag = new QDrag(this);
    drag->setPixmap(pixmap);
    drag->setHotSpot(pixmap.rect().center());
    drag->setMimeData(new QMimeData);

    emit dragStarted();
    const Qt::DropAction result = drag->exec(Qt::MoveAction);
    Q_UNUSED(result);

    m_draging = false;
    setVisible(true);
    update();
}

void PluginsItem::mouseClicked()
{
    const QString command = m_pluginInter->itemCommand(m_itemKey);
    if (!command.isEmpty())
    {
        QProcess *proc = new QProcess(this);

        connect(proc, static_cast<void (QProcess::*)(int)>(&QProcess::finished), proc, &QProcess::deleteLater);

        proc->startDetached(command);
    }
}
