// glyphpointnumbers.hpp

// Copyright (C) 2016-2022 by Werner Lemberg.


#pragma once

#include <QGraphicsItem>
#include <QPen>

#include <ft2build.h>
#include <freetype/freetype.h>
#include <freetype/ftoutln.h>


class GlyphPointNumbers
: public QGraphicsItem
{
public:
  GlyphPointNumbers(const QPen& onPen,
                    const QPen& offPen,
                    FT_Outline* outline);
  QRectF boundingRect() const;
  void paint(QPainter* painter,
             const QStyleOptionGraphicsItem* option,
             QWidget* widget);

private:
  QPen onPen;
  QPen offPen;
  FT_Outline* outline;
  QRectF bRect;
};


// end of glyphpointnumbers.hpp
