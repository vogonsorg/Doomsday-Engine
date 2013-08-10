/** @file documentwidget.cpp  Widget for displaying large amounts of text.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small> 
 */

#include "ui/widgets/documentwidget.h"
#include "ui/widgets/guirootwidget.h"
#include "ui/widgets/progresswidget.h"
#include "ui/widgets/gltextcomposer.h"

#include <de/Font>
#include <de/Drawable>
#include <de/Task>
#include <de/TaskPool>

using namespace de;

static int const ID_BACKGROUND = 1; // does not scroll
static int const ID_TEXT = 2;       // scrolls

DENG2_PIMPL(DocumentWidget),
DENG2_OBSERVES(Atlas, Reposition),
public Font::RichFormat::IStyle
{
    typedef DefaultVertexBuf VertexBuf;

    /**
     * Background task for wrapping text onto lines and figuring out the
     * formatting/tab stops.
     */
    class WrapTask : public Task
    {
    public:
        WrapTask(Instance *inst, int toWidth) : d(inst), _width(toWidth) {}
        void runTask() {
            DENG2_GUARD_FOR(d->wraps, G);
            d->wraps.wrapTextToWidth(d->text, d->format, _width);
            d->wrapTaskCompleted();
        }
    private:
        Instance *d;
        int _width;
    };

    ProgressWidget *progress;
    TaskPool tasks;

    // Style.
    ColorBank::Color normalColor;
    ColorBank::Color highlightColor;
    ColorBank::Color dimmedColor;
    ColorBank::Color accentColor;
    ColorBank::Color dimAccentColor;

    // State.
    ui::SizePolicy widthPolicy;
    int maxLineWidth;
    int oldScrollY;
    String styledText;
    String text;

    // GL objects.
    Font::RichFormat format;
    FontLineWrapping wraps;
    GLTextComposer composer;
    Drawable drawable;
    Matrix4f modelMatrix;
    GLUniform uMvpMatrix;
    GLUniform uScrollMvpMatrix;
    GLUniform uColor;
    GLState clippedTextState;

    Instance(Public *i)
        : Base(i),
          progress(0),
          widthPolicy(ui::Expand),
          maxLineWidth(1000),
          oldScrollY(0),
          format(*this),
          uMvpMatrix      ("uMvpMatrix", GLUniform::Mat4),
          uScrollMvpMatrix("uMvpMatrix", GLUniform::Mat4),
          uColor          ("uColor",     GLUniform::Vec4)
    {
        updateStyle();
        composer.setWrapping(wraps);

        // Widget to show while lines are being wrapped.
        progress = new ProgressWidget;
        progress->setColor("progress.dark.wheel");
        progress->setShadowColor("progress.dark.shadow");
        progress->rule().setRect(self.rule());
        progress->hide();
        self.add(progress);
    }

    ~Instance()
    {
        // Wait until background tasks finish.
        tasks.waitForDone();
    }

    bool isBeingWrapped() const
    {
        return !tasks.isDone();
    }

    void updateStyle()
    {
        Style const &st = self.style();

        normalColor    = st.colors().color("document.normal");
        highlightColor = st.colors().color("document.highlight");
        dimmedColor    = st.colors().color("document.dimmed");
        accentColor    = st.colors().color("document.accent");
        dimAccentColor = st.colors().color("document.dimaccent");

        wraps.setFont(self.font());
        wraps.clear();
        composer.forceUpdate();
        self.requestGeometry();
    }

    Font::RichFormat::IStyle::Color richStyleColor(int index) const
    {
        switch(index)
        {
        default:
        case Font::RichFormat::NormalColor:
            return normalColor;

        case Font::RichFormat::HighlightColor:
            return highlightColor;

        case Font::RichFormat::DimmedColor:
            return dimmedColor;

        case Font::RichFormat::AccentColor:
            return accentColor;

        case Font::RichFormat::DimAccentColor:
            return dimAccentColor;
        }
    }

    void richStyleFormat(int contentStyle,
                         float &sizeFactor,
                         Font::RichFormat::Weight &fontWeight,
                         Font::RichFormat::Style &fontStyle,
                         int &colorIndex) const
    {
        return self.style().richFormatStyle(contentStyle, sizeFactor, fontWeight, fontStyle, colorIndex);
    }

    void glInit()
    {        
        self.root().atlas().audienceForReposition += this;
        composer.setAtlas(self.root().atlas());
        composer.setText(text, format);

        self.setIndicatorUv(self.root().atlas().imageRectf(self.root().solidWhitePixel()).middle());

        drawable.addBuffer(ID_BACKGROUND, new VertexBuf);
        drawable.addBuffer(ID_TEXT,       new VertexBuf);

        self.root().shaders().build(drawable.program(), "generic.textured.color_ucolor")
                << uMvpMatrix << uColor << self.root().uAtlas();

        self.root().shaders().build(drawable.addProgram(ID_TEXT), "generic.textured.color_ucolor")
                << uScrollMvpMatrix << uColor << self.root().uAtlas();
        drawable.setProgram(ID_TEXT, drawable.program(ID_TEXT));
        drawable.setState(ID_TEXT, clippedTextState);
    }

    void glDeinit()
    {
        self.root().atlas().audienceForReposition -= this;
        composer.release();
        drawable.clear();
    }

    void atlasContentRepositioned(Atlas &atlas)
    {
        self.setIndicatorUv(atlas.imageRectf(self.root().solidWhitePixel()).middle());
        self.requestGeometry();
    }

    void beginWrapTask(int toWidth)
    {
        tasks.start(new WrapTask(this, toWidth));
    }

    void wrapTaskCompleted()
    {
        // This is executed in the background thread.
        progress->hide();        
        self.setContentSize(Vector2i(wraps.width(), wraps.totalHeightInPixels()));
    }

    void updateGeometry()
    {
        // If scroll position has changed, must update text geometry.
        int scrollY = self.scrollPositionY().valuei();
        if(oldScrollY != scrollY)
        {
            oldScrollY = scrollY;
            self.requestGeometry();
        }

        Rectanglei pos;
        if(self.hasChangedPlace(pos))
        {
            self.requestGeometry();
        }

        if(!self.geometryRequested()) return;

        int const margin = self.margin().valuei();

        // Background and scroll indicator.
        VertexBuf::Builder verts;
        self.glMakeGeometry(verts);
        drawable.buffer<VertexBuf>(ID_BACKGROUND).setVertices(gl::TriangleStrip, verts,
                                                              self.isScrolling()? gl::Dynamic : gl::Static);

        uMvpMatrix = self.root().projMatrix2D();

        if(!isBeingWrapped())
        {
            int wrapWidth;

            // Make sure the text has been wrapped for the current dimensions.
            if(widthPolicy == ui::Expand)
            {
                wrapWidth = maxLineWidth;
            }
            else
            {
                wrapWidth = self.rule().width().valuei() - 2 * margin;
            }

            if(wraps.isEmpty() || wraps.maximumWidth() != wrapWidth)
            {
                beginWrapTask(wrapWidth);
                return;
            }

            // Determine visible range of lines.
            Font const &font = self.font();
            int contentHeight = self.contentHeight();
            int const extraLines = 1;
            int numVisLines = contentHeight / font.lineSpacing().valuei() + 2 * extraLines;
            int firstVisLine = scrollY / font.lineSpacing().valuei() - extraLines + 1;

            // Update visible range and release/alloc lines accordingly.
            Rangei visRange(firstVisLine, firstVisLine + numVisLines);
            if(visRange != composer.range())
            {
                composer.setRange(visRange);
                composer.releaseLinesOutsideRange();
                composer.update();

                // Geometry from the text composer.
                if(composer.isReady())
                {
                    VertexBuf::Builder verts;
                    composer.makeVertices(verts, Vector2i(0, 0), ui::AlignLeft);
                    drawable.buffer<VertexBuf>(ID_TEXT).setVertices(gl::TriangleStrip, verts, gl::Static);
                }
            }

            uScrollMvpMatrix = self.root().projMatrix2D() *
                    Matrix4f::translate(Vector2f(self.contentRule().left().valuei(),
                                                 self.contentRule().top().valuei()));
        }

        // Geometry is now up to date.
        self.requestGeometry(false);
    }

    void draw()
    {
        updateGeometry();

        uColor = Vector4f(1, 1, 1, self.visibleOpacity());

        // Update the scissor for the text.
        clippedTextState = GLState::top();
        clippedTextState.setNormalizedScissor(self.normalizedContentRect());

        drawable.draw();
    }
};

DocumentWidget::DocumentWidget(String const &name) : d(new Instance(this))
{
    setWidthPolicy(ui::Expand);
}

void DocumentWidget::setText(String const &styledText)
{
    if(styledText != d->styledText)
    {
        // The wrapping task is uncancellable.
        d->tasks.waitForDone();

        // Show the progress indicator until the text is ready for drawing.
        d->drawable.buffer(ID_TEXT).clear();
        d->progress->show();
        int indSize = style().rules().rule("document.progress").valuei();
        setContentSize(Vector2i(indSize, indSize));

        d->wraps.clear();
        d->composer.release();

        d->styledText = styledText;
        d->text = d->format.initFromStyledText(styledText);
        d->composer.setText(d->text, d->format);

        requestGeometry();
    }
}

void DocumentWidget::setWidthPolicy(ui::SizePolicy policy)
{
    d->widthPolicy = policy;

    if(policy == ui::Expand)
    {
        rule().setInput(Rule::Width, contentRule().width() + 2 * margin());
    }
    else
    {
        rule().clearInput(Rule::Width);
    }

    requestGeometry();
}

void DocumentWidget::setMaximumLineWidth(int maxWidth)
{
    d->maxLineWidth = maxWidth;
    requestGeometry();
}

void DocumentWidget::viewResized()
{
    d->uMvpMatrix = root().projMatrix2D();
    requestGeometry();
}

void DocumentWidget::update()
{
    ScrollAreaWidget::update();
}

void DocumentWidget::drawContent()
{
    d->draw();
}

bool DocumentWidget::handleEvent(Event const &event)
{
    return ScrollAreaWidget::handleEvent(event);
}

void DocumentWidget::glInit()
{
    d->glInit();
}

void DocumentWidget::glDeinit()
{
    d->glDeinit();
}

void DocumentWidget::glMakeGeometry(DefaultVertexBuf::Builder &verts)
{
    ScrollAreaWidget::glMakeGeometry(verts);

    glMakeScrollIndicatorGeometry(verts, Vector2f(rule().left().value() + margin().value(),
                                                  rule().top().value()  + margin().value()));
}

void DocumentWidget::updateStyle()
{
    d->updateStyle();
}
