#pragma once

#include <QLayout>
#include <QList>
#include <algorithm>
#include <vector>

// SNR, frequency, lock: wrap only the frequency when the full row cannot fit.
class StatusLayout final : public QLayout
{
      public:
	explicit StatusLayout(QWidget *parent) : QLayout(parent)
	{
		setContentsMargins(12, 7, 9, 7);
		setSpacing(8);
	}
	~StatusLayout() override
	{
		while (auto *item = takeAt(0))
			delete item;
	}
	void addItem(QLayoutItem *item) override
	{
		items_.append(item);
		invalidate();
	}
	int count() const override
	{
		return items_.size();
	}
	QLayoutItem *itemAt(int index) const override
	{
		return items_.value(index);
	}
	QLayoutItem *takeAt(int index) override
	{
		return index >= 0 && index < count() ? items_.takeAt(index)
						     : nullptr;
	}
	bool hasHeightForWidth() const override
	{
		return true;
	}
	QSize minimumSize() const override
	{
		if (count() != 3)
			return {};
		const auto a = items_[0]->minimumSize();
		const auto b = items_[1]->minimumSize();
		const auto c = items_[2]->minimumSize();
		return QSize(
			   std::max(itemMinimumWidth(0), itemMinimumWidth(1)) +
			       spacing() + itemMinimumWidth(2),
			   std::max(a.height(),
				    std::max(b.height(), c.height()))) +
		       margins();
	}
	QSize sizeHint() const override
	{
		return QSize(rowWidth(), rowHeight()) + margins();
	}
	int heightForWidth(int width) const override
	{
		if (count() != 3)
			return margins().height();
		const int contentWidth = width - margins().width();
		const int height =
		    contentWidth >= rowWidth()
			? rowHeight()
			: std::max(itemHeight(0) + spacing() + itemHeight(1),
				   itemHeight(2));
		return height + margins().height();
	}
	void setGeometry(const QRect &rect) override
	{
		QLayout::setGeometry(rect);
		if (count() != 3)
			return;
		const QRect area = contentsRect();
		const int lockWidth = itemWidth(2);
		const int lockHeight = itemHeight(2);
		const bool single = area.width() >= rowWidth();
		const int lockX = area.right() + 1 - lockWidth;
		items_[2]->setGeometry(
		    QRect(lockX, area.y() + (area.height() - lockHeight) / 2,
			  lockWidth, lockHeight));
		if (single) {
			// Fixed floors keep the two values from shifting
			// sideways when their text changes.
			int x = area.x();
			for (int i = 0; i < 2; ++i) {
				const int width = itemWidth(i);
				const int height = itemHeight(i);
				items_[i]->setGeometry(QRect(
				    x, area.y() + (area.height() - height) / 2,
				    width, height));
				x += width + spacing();
			}
		} else {
			int y = area.y();
			for (int i = 0; i < 2; ++i) {
				const int height = itemHeight(i);
				items_[i]->setGeometry(QRect(
				    area.x(), y, lockX - spacing() - area.x(),
				    height));
				y += height + spacing();
			}
		}
	}

      private:
	QSize margins() const
	{
		const auto m = contentsMargins();
		return QSize(m.left() + m.right(), m.top() + m.bottom());
	}
	// SNR and Loop df grow and shrink with every value update. Wrapping on
	// the live text width would flip between one and two rows while the
	// window stays at one size, so each label keeps the widest width it has
	// shown as a floor. A new extreme can widen the row once; going back to
	// a shorter value cannot make it narrower again.
	int itemWidth(int index) const
	{
		if (index < 0 || index >= count())
			return 0;
		if (widthFloor_.size() != size_t(count()))
			widthFloor_.assign(size_t(count()), 0);
		widthFloor_[index] = std::max(
		    widthFloor_.at(index), items_[index]->sizeHint().width());
		return widthFloor_.at(index);
	}
	int itemMinimumWidth(int index) const
	{
		return std::max(itemWidth(index),
				items_[index]->minimumSize().width());
	}
	int itemHeight(int index) const
	{
		return index >= 0 && index < count()
			   ? items_[index]->sizeHint().height()
			   : 0;
	}
	int rowWidth() const
	{
		int width = std::max(0, count() - 1) * spacing();
		for (int index = 0; index < count(); ++index)
			width += itemWidth(index);
		return width;
	}
	int rowHeight() const
	{
		int height = 0;
		for (int index = 0; index < count(); ++index)
			height = std::max(height, itemHeight(index));
		return height;
	}
	QList<QLayoutItem *> items_;
	mutable std::vector<int> widthFloor_;
};
