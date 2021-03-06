#ifndef GEOMETRYITEM_HPP
#define GEOMETRYITEM_HPP

class GeometryItem : public QQuickItem {
    Q_OBJECT
public:
    GeometryItem(QQuickItem *parent = nullptr)
        : QQuickItem(parent) { m_size = size(); }
    auto setGeometry(const QPointF &pos, const QSizeF &size) -> void
        {setPosition(pos); setSize(size);}
    auto setGeometry(const QRectF &rect) -> void
        {setPosition(rect.topLeft()); setSize(rect.size());}
    auto size() const -> QSizeF { return {width(), height()}; }
    auto geometry() const -> QRectF { return {position(), size()}; }
    auto rect() const -> QRectF { return {0.0, 0.0, width(), height()}; }
signals:
    void sizeChanged(const QSizeF &size);
protected:
    auto geometryChanged(const QRectF &new_, const QRectF &o) -> void override;
private:
    QSizeF m_size;
};

#endif // GEOMETRYITEM_HPP
