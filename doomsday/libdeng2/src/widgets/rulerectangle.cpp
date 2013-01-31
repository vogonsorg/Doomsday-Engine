/*
 * The Doomsday Engine Project
 *
 * Copyright (c) 2011-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "de/RuleRectangle"
#include "de/DelegateRule"
#include "de/App"
#include "de/math.h"

namespace de {

struct RuleRectangle::Instance : public DelegateRule::ISource
{
    // Internal identifiers for the output rules.
    enum OutputIds
    {
        OutLeft,
        OutRight,
        OutWidth,

        OutTop,
        OutBottom,
        OutHeight,

        MAX_OUTPUT_RULES,

        // Ranges:
        FIRST_HORIZ_OUTPUT = OutLeft,
        LAST_HORIZ_OUTPUT  = OutWidth,
        FIRST_VERT_OUTPUT  = OutTop,
        LAST_VERT_OUTPUT   = OutBottom
    };

    AnimationVector2 normalizedAnchorPoint;
    Rule const *inputRules[MAX_INPUT_RULES];

    // The output rules.
    DelegateRule *outputRules[MAX_OUTPUT_RULES];

    Instance()
    {
        memset(inputRules, 0, sizeof(inputRules));
        setup();
    }

    Instance(Rule const *inLeft, Rule const *inTop, Rule const *inRight, Rule const *inBottom)
    {
        memset(inputRules, 0, sizeof(inputRules));
        inputRules[Left]   = inLeft;
        inputRules[Top]    = inTop;
        inputRules[Right]  = inRight;
        inputRules[Bottom] = inBottom;
        setup();
    }

    void setup()
    {
        // Create the output rules.
        for(int i = 0; i < int(MAX_OUTPUT_RULES); ++i)
        {
            outputRules[i] = new DelegateRule(*this, i);
        }

        // Depend on all specified input rules.
        for(int i = 0; i < int(MAX_INPUT_RULES); ++i)
        {
            connectInputToOutputs(InputRule(i), true);
        }
    }

    ~Instance()
    {
        for(int i = 0; i < int(MAX_INPUT_RULES); ++i)
        {
            connectInputToOutputs(InputRule(i), false);
        }
        for(int i = 0; i < int(MAX_OUTPUT_RULES); ++i)
        {
            outputRules[i]->setSource(0);
            releaseRef(outputRules[i]);
        }
    }

    Rule const *&ruleRef(InputRule rule)
    {
        DENG2_ASSERT(rule >= Left);
        DENG2_ASSERT(rule < MAX_INPUT_RULES);

        return inputRules[rule];
    }

    void invalidateOutputs()
    {
        for(int i = 0; i < int(MAX_OUTPUT_RULES); ++i)
        {
            outputRules[i]->invalidate();
        }
    }

    void connectInputToOutputs(InputRule inputRule, bool doConnect)
    {
        Rule const *&input = ruleRef(inputRule);
        if(!input) return;

        bool isHoriz = (inputRule == Left  || inputRule == Right ||
                        inputRule == Width || inputRule == AnchorX);

        int const start = (isHoriz? FIRST_HORIZ_OUTPUT : FIRST_VERT_OUTPUT);
        int const end   = (isHoriz? LAST_HORIZ_OUTPUT  : LAST_VERT_OUTPUT );

        for(int i = start; i <= end; ++i)
        {
            if(doConnect)
            {
                outputRules[i]->dependsOn(input);
                outputRules[i]->invalidate();
            }
            else
            {
                outputRules[i]->independentOf(input);
            }
        }
    }

    void setInputRule(InputRule inputRule, Rule const &rule)
    {
        // Disconnect the old input rule from relevant outputs.
        connectInputToOutputs(inputRule, false);

        ruleRef(inputRule) = &rule;

        // Connect to relevant outputs.
        connectInputToOutputs(inputRule, true);
    }

    void updateWidth()
    {
        if(inputRules[Width])
        {
            outputRules[OutWidth]->set(inputRules[Width]->value());
        }
        else
        {
            // Need to calculate width using edges.
            updateHorizontal();
        }
    }

    void updateHorizontal()
    {
        // Both edges must be defined, otherwise the rectangle's position is ambiguous.
        bool leftDefined   = false;
        bool rightDefined  = false;

        Rectanglef r;

        if(inputRules[AnchorX] && inputRules[Width])
        {
            r.topLeft.x = inputRules[AnchorX]->value() -
                    normalizedAnchorPoint.x * inputRules[Width]->value();
            r.setWidth(inputRules[Width]->value());
            leftDefined = rightDefined = true;
        }

        if(inputRules[Left])
        {
            r.topLeft.x = inputRules[Left]->value();
            leftDefined = true;
        }
        if(inputRules[Right])
        {
            r.bottomRight.x = inputRules[Right]->value();
            rightDefined = true;
        }

        if(inputRules[Width] && leftDefined && !rightDefined)
        {
            r.setWidth(inputRules[Width]->value());
            rightDefined = true;
        }
        if(inputRules[Width] && !leftDefined && rightDefined)
        {
            r.topLeft.x = r.bottomRight.x - inputRules[Width]->value();
            leftDefined = true;
        }

        DENG2_ASSERT(leftDefined);
        DENG2_ASSERT(rightDefined);

        outputRules[OutLeft]->set(r.topLeft.x);
        outputRules[OutRight]->set(r.bottomRight.x);
        outputRules[OutWidth]->set(r.width());
    }

    void updateHeight()
    {
        if(inputRules[Height])
        {
            outputRules[OutHeight]->set(inputRules[Height]->value());
        }
        else
        {
            // Need to calculate width using edges.
            updateVertical();
        }
    }

    void updateVertical()
    {
        // Both edges must be defined, otherwise the rectangle's position is ambiguous.
        bool topDefined    = false;
        bool bottomDefined = false;

        Rectanglef r;

        if(inputRules[AnchorY] && inputRules[Height])
        {
            r.topLeft.y = inputRules[AnchorY]->value() -
                    normalizedAnchorPoint.y * inputRules[Height]->value();
            r.setHeight(inputRules[Height]->value());
            topDefined = bottomDefined = true;
        }

        if(inputRules[Top])
        {
            r.topLeft.y = inputRules[Top]->value();
            topDefined = true;
        }
        if(inputRules[Bottom])
        {
            r.bottomRight.y = inputRules[Bottom]->value();
            bottomDefined = true;
        }

        if(inputRules[Height] && topDefined && !bottomDefined)
        {
            r.setHeight(inputRules[Height]->value());
            bottomDefined = true;
        }
        if(inputRules[Height] && !topDefined && bottomDefined)
        {
            r.topLeft.y = r.bottomRight.y - inputRules[Height]->value();
            topDefined = true;
        }

        DENG2_ASSERT(topDefined);
        DENG2_ASSERT(bottomDefined);

        // Update the derived output rules.
        outputRules[OutTop]->set(r.topLeft.y);
        outputRules[OutBottom]->set(r.bottomRight.y);
        outputRules[OutHeight]->set(r.height());
    }

    // Implements DelegateRule::ISource.
    void delegateUpdate(int id)
    {
        switch(id)
        {
        case OutLeft:
        case OutRight:
            updateHorizontal();
            break;

        case OutWidth:
            updateWidth();
            break;

        case OutTop:
        case OutBottom:
            updateVertical();
            break;

        case OutHeight:
            updateHeight();
            break;
        }
    }

    void delegateInvalidation(int id)
    {
        // Due to the intrinsic relationships between the outputs (as edges of
        // a rectangle), invalidation of one may cause others to become
        // invalid, too.
        switch(id)
        {
        case OutLeft:
        case OutRight:
            outputRules[OutWidth]->invalidate();
            break;

        case OutWidth:
            outputRules[OutLeft]->invalidate();
            outputRules[OutRight]->invalidate();
            break;

        case OutTop:
        case OutBottom:
            outputRules[OutHeight]->invalidate();
            break;

        case OutHeight:
            outputRules[OutTop]->invalidate();
            outputRules[OutBottom]->invalidate();
            break;
        }
    }
};

RuleRectangle::RuleRectangle() : d(new Instance)
{}

RuleRectangle::RuleRectangle(Rule const &left, Rule const &top, Rule const &right, Rule const &bottom)
    : d(new Instance(&left, &top, &right, &bottom))
{}

RuleRectangle::RuleRectangle(RuleRectangle const &rect)
    : d(new Instance(&rect.left(), &rect.top(), &rect.right(), &rect.bottom()))
{}

RuleRectangle::~RuleRectangle()
{
    delete d;
}

Rule const &RuleRectangle::left() const
{
    return *d->outputRules[Instance::OutLeft];
}

Rule const &RuleRectangle::top() const
{
    return *d->outputRules[Instance::OutTop];
}

Rule const &RuleRectangle::right() const
{
    return *d->outputRules[Instance::OutRight];
}

Rule const &RuleRectangle::bottom() const
{
    return *d->outputRules[Instance::OutBottom];
}

Rule const &RuleRectangle::width() const
{
    return *d->outputRules[Instance::OutWidth];
}

Rule const &RuleRectangle::height() const
{
    return *d->outputRules[Instance::OutHeight];
}

RuleRectangle &RuleRectangle::setInput(InputRule inputRule, Rule const &rule)
{
    d->setInputRule(inputRule, rule);
    return *this;
}

Rule const &RuleRectangle::inputRule(InputRule inputRule)
{
    return *d->ruleRef(inputRule);
}

void RuleRectangle::setAnchorPoint(Vector2f const &normalizedPoint, TimeDelta const &transition)
{
    d->normalizedAnchorPoint.setValue(normalizedPoint, transition);
    d->invalidateOutputs();

    if(transition > 0.0)
    {
        // Animation started, keep an eye on the clock until it ends.
        Clock::appClock().audienceForTimeChange += this;
    }
}

void RuleRectangle::timeChanged(Clock const &clock)
{
    d->invalidateOutputs();

    if(d->normalizedAnchorPoint.done())
    {
        clock.audienceForTimeChange -= this;
    }
}

Rectanglef RuleRectangle::rect() const
{
    return Rectanglef(Vector2f(left().value(),  top().value()),
                      Vector2f(right().value(), bottom().value()));
}

Rectanglei RuleRectangle::recti() const
{
    Rectanglef const r = rect();
    return Rectanglei(Vector2i(de::floor(r.topLeft.x),     de::floor(r.topLeft.y)),
                      Vector2i(de::floor(r.bottomRight.x), de::floor(r.bottomRight.y)));
}

} // namespace de
