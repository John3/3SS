//-----------------------------------------------------------------------------
// Torque
// Copyright GarageGames, LLC 2011
//-----------------------------------------------------------------------------

#include "console/consoleTypes.h"
#include "console/console.h"
#include "console/consoleInternal.h"
#include "console/codeBlock.h"
#include "platform/event.h"
#include "graphics/gBitmap.h"
#include "graphics/dgl.h"
#include "input/actionMap.h"
#include "gui/guiCanvas.h"
#include "gui/guiControl.h"
#include "gui/guiDefaultControlRender.h"
#include "gui/editor/guiEditCtrl.h"
#include "string/unicode.h"
#include "collection/vector.h"

#ifndef _FRAMEALLOCATOR_H_
#include "memory/frameAllocator.h"
#endif

//------------------------------------------------------------------------------

IMPLEMENT_CONOBJECT(GuiControl);

//used to locate the next/prev responder when tab is pressed
S32 GuiControl::smCursorChanged           = -1;
GuiControl *GuiControl::smPrevResponder = NULL;
GuiControl *GuiControl::smCurResponder = NULL;

GuiEditCtrl *GuiControl::smEditorHandle = NULL;

bool GuiControl::smDesignTime = false;

GuiControl::GuiControl()
{
   mLayer = 0;
   mBounds.set(0, 0, 64, 64);
   mMinExtent.set(8, 2);			// MM: Reduced to 8x2 so GuiControl can be used as a seperator.

   mProfile = NULL;

   mConsoleVariable     = StringTable->EmptyString;
   mConsoleCommand      = StringTable->EmptyString;
   mAltConsoleCommand   = StringTable->EmptyString;
   mAcceleratorKey      = StringTable->EmptyString;
   mLangTableName       = StringTable->EmptyString;

#ifdef TORQUE_PLAYER
   mScriptFile          = CodeBlock::getCurrentCodeBlockName();
#else
   mScriptFile          = CodeBlock::getCurrentCodeBlockFullPath();
#endif

   mLangTable           = NULL;
   mFirstResponder      = NULL;
   mCanSaveFieldDictionary = false;
   mVisible             = true;
   mActive              = false;
   mAwake               = false;
   mCanSave				= true;
   mHorizSizing         = horizResizeRight;
   mVertSizing          = vertResizeBottom;
   mTooltipProfile      = NULL;
   mTooltip             = StringTable->EmptyString;
   mTipHoverTime        = 1000;
   mTooltipWidth		= 250;
   mIsContainer         = false;

   mNSLinkMask = LinkSuperClassName | LinkClassName;
}

GuiControl::~GuiControl()
{
}

bool GuiControl::onAdd()
{
   // Let Parent Do Work.
   if(!Parent::onAdd())
      return false;

   // Grab the classname of this object
   const char *cName = getClassName();

   // if we're a pure GuiControl, then we're a container by default.
   if(dStrcmp("GuiControl", cName) == 0)
      mIsContainer = true;

   // Clamp to minExtent
   mBounds.extent.x = getMax( mMinExtent.x, mBounds.extent.x );
   mBounds.extent.y = getMax( mMinExtent.y, mBounds.extent.y );


   // Add to root group.
   Sim::getGuiGroup()->addObject(this);

   // Notify Script.
   if( isMethod("onAdd") )
      Con::executef(this, 1, "onAdd");

   // Return Success.
   return true;
}

void GuiControl::onChildAdded( GuiControl *child )
{
   // Base class does not make use of this
}

static EnumTable::Enums horzEnums[] =
{
    { GuiControl::horizResizeRight,      "right"     },
    { GuiControl::horizResizeWidth,      "width"     },
    { GuiControl::horizResizeLeft,       "left"      },
   { GuiControl::horizResizeCenter,     "center"    },
   { GuiControl::horizResizeRelative,   "relative"  }
};
static EnumTable gHorizSizingTable(5, &horzEnums[0]);

static EnumTable::Enums vertEnums[] =
{
    { GuiControl::vertResizeBottom,      "bottom"     },
    { GuiControl::vertResizeHeight,      "height"     },
    { GuiControl::vertResizeTop,         "top"        },
   { GuiControl::vertResizeCenter,      "center"     },
   { GuiControl::vertResizeRelative,    "relative"   }
};
static EnumTable gVertSizingTable(5, &vertEnums[0]);

void GuiControl::initPersistFields()
{
   Parent::initPersistFields();


   // Things relevant only to the editor.
   addGroup("Gui Editing");
   addField("isContainer",       TypeBool,      Offset(mIsContainer, GuiControl));
   endGroup("Gui Editing");

   // Parent Group.
   addGroup("GuiControl");

   addField("Profile",           TypeGuiProfile,	Offset(mProfile, GuiControl));
   addField("HorizSizing",       TypeEnum,			Offset(mHorizSizing, GuiControl), 1, &gHorizSizingTable);
   addField("VertSizing",        TypeEnum,			Offset(mVertSizing, GuiControl), 1, &gVertSizingTable);

   addField("Position",          TypePoint2I,		Offset(mBounds.point, GuiControl));
   addField("Extent",            TypePoint2I,		Offset(mBounds.extent, GuiControl));
   addField("MinExtent",         TypePoint2I,		Offset(mMinExtent, GuiControl));
   addField("canSave",           TypeBool,			Offset(mCanSave, GuiControl));
   addField("Visible",           TypeBool,			Offset(mVisible, GuiControl));
   addDepricatedField("Modal");
   addDepricatedField("SetFirstResponder");

   addField("Variable",          TypeString,		Offset(mConsoleVariable, GuiControl));
   addField("Command",           TypeString,		Offset(mConsoleCommand, GuiControl));
   addField("AltCommand",        TypeString,		Offset(mAltConsoleCommand, GuiControl));
   addField("Accelerator",       TypeString,		Offset(mAcceleratorKey, GuiControl));
   addField("Active",			 TypeBool,			Offset(mActive, GuiControl));
   endGroup("GuiControl");	

   addGroup("ToolTip");
   addField("tooltipprofile",    TypeGuiProfile,	Offset(mTooltipProfile, GuiControl));
   addField("tooltip",           TypeString,		Offset(mTooltip, GuiControl));
   addField("tooltipWidth",      TypeS32,			Offset(mTooltipWidth, GuiControl));
   addField("hovertime",         TypeS32,			Offset(mTipHoverTime, GuiControl));
   endGroup("ToolTip");


   addGroup("Localization");
   addField("langTableMod",      TypeString,		Offset(mLangTableName, GuiControl));
   endGroup("Localization");
}

// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- //

LangTable * GuiControl::getGUILangTable()
{
    if(mLangTable)
        return mLangTable;

    if(mLangTableName && *mLangTableName)
    {
        mLangTable = (LangTable *)getModLangTable((const UTF8*)mLangTableName);
        return mLangTable;
    }

    GuiControl *parent = getParent();
    if(parent)
        return parent->getGUILangTable();

    return NULL;
}

const UTF8 * GuiControl::getGUIString(S32 id)
{
    LangTable *lt = getGUILangTable();
    if(lt)
        return lt->getString(id);

    return NULL;
}

// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- //


void GuiControl::addObject(SimObject *object)
{
   GuiControl *ctrl = dynamic_cast<GuiControl *>(object);
   if(!ctrl)
   {
      AssertWarn(0, "GuiControl::addObject: attempted to add NON GuiControl to set");
      return;
   }

   if(object->getGroup() == this)
      return;

    Parent::addObject(object);

   AssertFatal(!ctrl->isAwake(), "GuiControl::addObject: object is already awake before add");
   if(mAwake)
      ctrl->awaken();

  // If we are a child, notify our parent that we've been removed
  GuiControl *parent = ctrl->getParent();
  if( parent )
     parent->onChildAdded( ctrl );


}

void GuiControl::removeObject(SimObject *object)
{
   AssertFatal(mAwake == static_cast<GuiControl*>(object)->isAwake(), "GuiControl::removeObject: child control wake state is bad");
   if (mAwake)
      static_cast<GuiControl*>(object)->sleep();
    Parent::removeObject(object);
}

GuiControl *GuiControl::getParent()
{
    SimObject *obj = getGroup();
    if (GuiControl* gui = dynamic_cast<GuiControl*>(obj))
      return gui;
   return 0;
}

GuiCanvas *GuiControl::getRoot()
{
   GuiControl *root = NULL;
    GuiControl *parent = getParent();
   while (parent)
   {
      root = parent;
      parent = parent->getParent();
   }
   if (root)
      return dynamic_cast<GuiCanvas*>(root);
   else
      return NULL;
}

void GuiControl::inspectPreApply()
{
   if(smDesignTime && smEditorHandle)
      smEditorHandle->controlInspectPreApply(this);
   
   // The canvas never sleeps
   if(mAwake && dynamic_cast<GuiCanvas*>(this) == NULL )
   {
      onSleep(); // release all our resources.
      mAwake = true;
   }
}

void GuiControl::inspectPostApply()
{
   // Shhhhhhh, you don't want to wake the canvas!
   if(mAwake && dynamic_cast<GuiCanvas*>(this) == NULL )
   {
      mAwake = false;
      onWake();
   }
   
   if(smDesignTime && smEditorHandle)
      smEditorHandle->controlInspectPostApply(this);
}

// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- //

Point2I GuiControl::localToGlobalCoord(const Point2I &src)
{
   Point2I ret = src;
   ret += mBounds.point;
   GuiControl *walk = getParent();
   while(walk)
   {
      ret += walk->getPosition();
      walk = walk->getParent();
   }
   return ret;
}

Point2I GuiControl::globalToLocalCoord(const Point2I &src)
{
   Point2I ret = src;
   ret -= mBounds.point;
   GuiControl *walk = getParent();
   while(walk)
   {
      ret -= walk->getPosition();
      walk = walk->getParent();
   }
   return ret;
}

//----------------------------------------------------------------
void GuiControl::resize(const Point2I &newPosition, const Point2I &newExtent)
{
   Point2I actualNewExtent = Point2I(getMax(mMinExtent.x, newExtent.x),
      getMax(mMinExtent.y, newExtent.y));

   // only do the child control resizing stuff if you really need to.
   bool extentChanged = (actualNewExtent != mBounds.extent);

   if (extentChanged) {
      //call set update both before and after
      setUpdate();
      iterator i;
      for(i = begin(); i != end(); i++)
      {
         GuiControl *ctrl = static_cast<GuiControl *>(*i);
         ctrl->parentResized(mBounds.extent, actualNewExtent);
      }
      mBounds.set(newPosition, actualNewExtent);

      GuiControl *parent = getParent();
      if (parent)
         parent->childResized(this);
      setUpdate();
   }
   else {
      mBounds.point = newPosition;
   }
}
void GuiControl::setPosition( const Point2I &newPosition )
{
   resize( newPosition, mBounds.extent );
}

void GuiControl::setExtent( const Point2I &newExtent )
{
   resize( mBounds.point, newExtent );
}

void GuiControl::setBounds( const RectI &newBounds )
{
   resize( newBounds.point, newBounds.extent );
}

void GuiControl::setLeft( S32 newLeft )
{
   resize( Point2I( newLeft, mBounds.point.y), mBounds.extent );
}

void GuiControl::setTop( S32 newTop )
{
   resize( Point2I( mBounds.point.x, newTop ), mBounds.extent );
}

void GuiControl::setWidth( S32 newWidth )
{
   resize( mBounds.point, Point2I( newWidth, mBounds.extent.y ) );
}

void GuiControl::setHeight( S32 newHeight )
{
   resize( mBounds.point, Point2I( mBounds.extent.x, newHeight ) );
}

void GuiControl::childResized(GuiControl *child)
{
   // default to do nothing...
}

void GuiControl::parentResized(const Point2I &oldParentExtent, const Point2I &newParentExtent)
{
   Point2I newPosition = getPosition();
   Point2I newExtent = getExtent();

    S32 deltaX = newParentExtent.x - oldParentExtent.x;
    S32 deltaY = newParentExtent.y - oldParentExtent.y;

    if (mHorizSizing == horizResizeCenter)
       newPosition.x = (newParentExtent.x - mBounds.extent.x) >> 1;
    else if (mHorizSizing == horizResizeWidth)
        newExtent.x += deltaX;
    else if (mHorizSizing == horizResizeLeft)
      newPosition.x += deltaX;
   else if (mHorizSizing == horizResizeRelative && oldParentExtent.x != 0)
   {
      S32 newLeft = (newPosition.x * newParentExtent.x) / oldParentExtent.x;
      S32 newRight = ((newPosition.x + newExtent.x) * newParentExtent.x) / oldParentExtent.x;

      newPosition.x = newLeft;
      newExtent.x = newRight - newLeft;
   }

    if (mVertSizing == vertResizeCenter)
       newPosition.y = (newParentExtent.y - mBounds.extent.y) >> 1;
    else if (mVertSizing == vertResizeHeight)
        newExtent.y += deltaY;
    else if (mVertSizing == vertResizeTop)
      newPosition.y += deltaY;
   else if(mVertSizing == vertResizeRelative && oldParentExtent.y != 0)
   {

      S32 newTop = (newPosition.y * newParentExtent.y) / oldParentExtent.y;
      S32 newBottom = ((newPosition.y + newExtent.y) * newParentExtent.y) / oldParentExtent.y;

      newPosition.y = newTop;
      newExtent.y = newBottom - newTop;
   }

   // Resizing Re factor [9/18/2006]
   // Only resize if our minExtent is satisfied with it.
   //if( newExtent.x >= mMinExtent.x && newExtent.y >= mMinExtent.y )
      resize(newPosition, newExtent);
}

//----------------------------------------------------------------

void GuiControl::onRender(Point2I offset, const RectI &updateRect)
{
    RectI ctrlRect(offset, mBounds.extent);

    dglSetBitmapModulation( mProfile->mFontColor );
    //if opaque, fill the update rect with the fill color
    if (mProfile->mOpaque)
        dglDrawRectFill( ctrlRect, mProfile->mFillColor );

    //if there's a border, draw the border
    if (mProfile->mBorder)
        renderBorder(ctrlRect, mProfile);

    renderChildControls(offset, updateRect);
}

bool GuiControl::renderTooltip(Point2I cursorPos, const char* tipText )
{
    // Short Circuit.
    if (!mAwake) 
        return false;

    if ( dStrlen( mTooltip ) == 0 && ( tipText == NULL || dStrlen( tipText ) == 0 ) )
        return false;

    const char* renderTip = mTooltip;
    if( tipText != NULL )
        renderTip = tipText;

    // Finish if no root.
    GuiCanvas *root = getRoot();
    if ( !root )
        return false;

    if (!mTooltipProfile)
        mTooltipProfile = mProfile;

    GFont *font = mTooltipProfile->mFont;
   
    // Set text bounds.
    Point2I textBounds( 0, 0 );

    // Fetch the width of a space.
    const S32 spaceWidth = (S32)font->getStrWidth(" ");

    // Fetch the maximum allowed tooltip extent.
    const S32 maxTooltipWidth = mTooltipWidth;

    // Fetch word count.
    const S32 wordCount = StringUnit::getUnitCount( renderTip, " " );

    // Reset line storage.
    const S32 tooltipLineStride = (S32)font->getHeight() + 4;
    const S32 maxTooltipLines = 20;
    S32 tooltipLineCount = 0;
    S32 tooltipLineWidth = 0;
    FrameTemp<StringBuffer> tooltipLines( maxTooltipLines );

    // Reset word indexing.
    S32 wordStartIndex = 0;
    S32 wordEndIndex = 0;

    // Search for end word.
    while( true )
    {
        // Do we have any words left?
        if ( wordEndIndex < wordCount )
        {
            // Yes, so fetch the word.
            const char* pWord = StringUnit::getUnit( renderTip, wordEndIndex, " " );

            // Add word length.
            const S32 wordLength = (S32)font->getStrWidth( pWord ) + spaceWidth;

            // Do we still have room?
            if ( (tooltipLineWidth + wordLength) < maxTooltipWidth )
            {
                // Yes, so add word length.
                tooltipLineWidth += wordLength;

                // Next word.
                wordEndIndex++;

                continue;
            }

            // Do we have any lines left?
            if ( tooltipLineCount < maxTooltipLines )
            {
                // Yes, so insert line.
                tooltipLines[tooltipLineCount++] = StringUnit::getUnits( renderTip, wordStartIndex, wordEndIndex-1, " " );

                // Update horizontal text bounds.
                if ( tooltipLineWidth > textBounds.x )
                    textBounds.x = tooltipLineWidth;
            }

            // Set new line length.
            tooltipLineWidth = wordLength;

            // Set word start.
            wordStartIndex = wordEndIndex;

            // Next word.
            wordEndIndex++;

            continue;
        }

        // Do we have any words left?
        if ( wordStartIndex < wordCount )
        {
            // Yes, so do we have any lines left?
            if ( tooltipLineCount < maxTooltipLines )
            {
                // Yes, so insert line.
                tooltipLines[tooltipLineCount++] = StringUnit::getUnits( renderTip, wordStartIndex, wordCount-1, " " );

                // Update horizontal text bounds.
                if ( tooltipLineWidth > textBounds.x )
                    textBounds.x = tooltipLineWidth;
            }
        }

        break;
    }

    // Controls the size of the inside (gutter) tooltip region.
    const S32 tooltipGutterSize = 5;

    // Adjust text bounds.
    textBounds.x += tooltipGutterSize * 2;
    textBounds.y = (((S32)font->getHeight() + 4) * tooltipLineCount - 4) + (tooltipGutterSize * 2);

    // Adjust to tooltip is always on-screen.
    Point2I screensize = Platform::getWindowSize();
    Point2I offset = cursorPos; 
    offset.y += 22;
    if (screensize.x < (offset.x + textBounds.x))
        offset.x = screensize.x - textBounds.x;
    if(screensize.y < (offset.y + textBounds.y) )
        offset.y = screensize.y - textBounds.y;

    // Fetch the old clip.
    RectI oldClip = dglGetClipRect();

    // Set rectangle for the box, and set the clip rectangle.
    RectI rect(offset, textBounds);
    dglSetClipRect(rect);

    // Draw Filler bit, then border on top of that
    dglDrawRectFill(rect, mTooltipProfile->mFillColor );
    dglDrawRect( rect, mTooltipProfile->mBorderColor );

    // Draw the text centered in the tool tip box
    dglSetBitmapModulation( mTooltipProfile->mFontColor );
    Point2I start( tooltipGutterSize, tooltipGutterSize );
    for ( S32 lineIndex = 0; lineIndex < tooltipLineCount; lineIndex++ )
    {
        dglDrawText( font, start + offset, tooltipLines[lineIndex].getPtr8(), mProfile->mFontColors );
        offset.y += tooltipLineStride;
    }

    dglSetClipRect( oldClip );
    return true;
}

void GuiControl::renderChildControls(Point2I offset, const RectI &updateRect)
{
   // offset is the upper-left corner of this control in screen coordinates
   // updateRect is the intersection rectangle in screen coords of the control
   // hierarchy.  This can be set as the clip rectangle in most cases.
   RectI clipRect = updateRect;

   S32 size = objectList.size();
   S32 size_cpy = size;
   // UNUSED: JOSEPH THOMAS -> GuiControl *ctrl = NULL;
    //-Mat look through our vector all normal-like, trying to use an iterator sometimes gives us
   //bad cast on good objects
   for( S32 count = 0; count < objectList.size(); count++ )
   {
      GuiControl *ctrl = (GuiControl *)objectList[count];
      if( ctrl == NULL ) {
          Con::errorf( "GuiControl::renderChildControls() object %i is NULL", count );
        continue;
      }
      if (ctrl->mVisible)
      {
         Point2I childPosition = offset + ctrl->getPosition();
         RectI childClip(childPosition, ctrl->getExtent());

         if (childClip.intersect(clipRect))
         {
            dglSetClipRect(childClip);
            glDisable(GL_CULL_FACE);
            ctrl->onRender(childPosition, childClip);
         }
      }
      size_cpy = objectList.size(); //	CHRIS: i know its wierd but the size of the list changes sometimes during execution of this loop
      if(size != size_cpy)
      {
          size = size_cpy;
          count--;	//	CHRIS: just to make sure one wasnt skipped.
      }
   }
}

void GuiControl::setUpdateRegion(Point2I pos, Point2I ext)
{
   Point2I upos = localToGlobalCoord(pos);
   GuiCanvas *root = getRoot();
   if (root)
   {
      root->addUpdateRegion(upos, ext);
   }
}

void GuiControl::setUpdate()
{
   setUpdateRegion(Point2I(0,0), mBounds.extent);
}

// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- //

void GuiControl::awaken()
{
   AssertFatal(!mAwake, "GuiControl::awaken: control is already awake");
   if(mAwake)
      return;

   iterator i;
   for(i = begin(); i != end(); i++)
   {
      GuiControl *ctrl = static_cast<GuiControl *>(*i);

      AssertFatal(!ctrl->isAwake(), "GuiControl::awaken: child control is already awake");
      if(!ctrl->isAwake())
         ctrl->awaken();
   }

   AssertFatal(!mAwake, "GuiControl::awaken: should not be awake here");
   if(!mAwake)
   {
      if(!onWake())
      {
         Con::errorf(ConsoleLogEntry::General, "GuiControl::awaken: failed onWake for obj: %s", getName());
         AssertFatal(0, "GuiControl::awaken: failed onWake");
         deleteObject();
      }
   }
}

void GuiControl::sleep()
{
   AssertFatal(mAwake, "GuiControl::sleep: control is not awake");
   if(!mAwake)
      return;

   iterator i;
   for(i = begin(); i != end(); i++)
   {
      GuiControl *ctrl = static_cast<GuiControl *>(*i);

      AssertFatal(ctrl->isAwake(), "GuiControl::sleep: child control is already asleep");
      if(ctrl->isAwake())
         ctrl->sleep();
   }

   AssertFatal(mAwake, "GuiControl::sleep: should not be asleep here");
   if(mAwake)
      onSleep();
}

void GuiControl::preRender()
{
   AssertFatal(mAwake, "GuiControl::preRender: control is not awake");
   if(!mAwake)
      return;

   iterator i;
   for(i = begin(); i != end(); i++)
   {
      GuiControl *ctrl = static_cast<GuiControl *>(*i);
      ctrl->preRender();
   }
   onPreRender();
}

// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- //

bool GuiControl::onWake()
{
   AssertFatal( !mAwake, "GuiControl::onWake: control is already awake" );
   if( mAwake )
      return false;

   // [tom, 4/18/2005] Cause mLangTable to be refreshed in case it was changed
   mLangTable = NULL;

   // Grab the classname of this object
   const char *cName = getClassName();

   //make sure we have a profile
   if( !mProfile )
   {
      // Ensure the classname is a valid name...
      if( cName && cName[0] )
      {
         S32 pos = 0;

         for( pos = 0; pos <= (S32)dStrlen( cName ); pos++ )
            if( !dStrncmp( cName + pos, "Ctrl", 4 ) )
               break;

         if( pos != 0 ) {
            char buff[255];
            dStrncpy( buff, cName, pos );
            buff[pos] = '\0';
            dStrcat( buff, "Profile\0" );

            SimObject *obj = Sim::findObject( buff );

            if( obj )
               mProfile = dynamic_cast<GuiControlProfile*>( obj );
         }
      }

      // Ok lets check to see if that worked
      if( !mProfile ) {
         SimObject *obj = Sim::findObject( "GuiDefaultProfile" );

         if( obj )
            mProfile = dynamic_cast<GuiControlProfile*>(obj);
      }

      AssertFatal( mProfile, avar( "GuiControl: %s created with no profile.", getName() ) );
   }

   //set the flag
   mAwake = true;

   //set the layer
   GuiCanvas *root = getRoot();
   AssertFatal(root, "Unable to get the root Canvas.");
   GuiControl *parent = getParent();
   if (parent && parent != root)
      mLayer = parent->mLayer;

   //make sure the first responder exists
   if (! mFirstResponder)
      mFirstResponder = findFirstTabable();

   //see if we should force this control to be the first responder
   //if (mProfile->mTabable && mProfile->mCanKeyFocus)
   //   setFirstResponder();

   //increment the profile
   mProfile->incRefCount();

   // Only invoke script callbacks if we have a namespace in which to do so
   // This will suppress warnings
   if( isMethod("onWake") )
      Con::executef(this, 1, "onWake");

   return true;
}

void GuiControl::onSleep()
{
   AssertFatal(mAwake, "GuiControl::onSleep: control is not awake");
   if(!mAwake)
      return;

   //decrement the profile referrence
   if( mProfile != NULL )
      mProfile->decRefCount();
   clearFirstResponder();
   mouseUnlock();

   // Only invoke script callbacks if we have a namespace in which to do so
   // This will suppress warnings
   if( isMethod("onSleep") )
      Con::executef(this, 1, "onSleep");

   // Set Flag
   mAwake = false;
}

void GuiControl::setControlProfile(GuiControlProfile *prof)
{
   AssertFatal(prof, "GuiControl::setControlProfile: invalid profile");
   if(prof == mProfile)
      return;
   if(mAwake)
      mProfile->decRefCount();
   mProfile = prof;
   if(mAwake)
      mProfile->incRefCount();

}

void GuiControl::onPreRender()
{
   // do nothing.
}
//-----------------------------------------------------------------------------
//	checks up the parent hierarchy - if anyone above us is not savable returns false
//	otherwise, returns true.
//-----------------------------------------------------------------------------
bool	GuiControl::getCanSaveParent()
{
   GuiControl *walk = this;
   while(walk)
   {
      if(!walk->getCanSave())
         return false;

      walk = walk->getParent();
   }

   return true;
}

//-----------------------------------------------------------------------------
//	Can we Save to a TorqueScript file?
//-----------------------------------------------------------------------------
bool GuiControl::getCanSave()
{
   return mCanSave;
}

//-----------------------------------------------------------------------------
//	Sets whether we can save out to a file (TorqueScript)
//-----------------------------------------------------------------------------
void GuiControl::setCanSave(bool bCanSave)
{
   mCanSave	=	bCanSave;
}

ConsoleMethod( GuiControl, setCanSave, void, 3,3,"(bool canSave) Sets whether this control can serialize itself to the hard disk\n"
              "@param Flag setting\n"
              "@return No Return Value")
{
   object->setCanSave( dAtob( argv[2] ) );
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//	checks out mCanSave flag, if true just passes along to our parent, 
//	if false, then we return without writing.  Note, also, that 
//	if our parent is not writeable, then we should not be writable...
////////////////////////////////////////////////////////////////////////////////////////////////////
void GuiControl::write(Stream &stream, U32 tabStop, U32 flags)
{
   // UNUSED: JOSEPH THOMAS -> GuiControl* pParent	=	getParent();
   //note: this will return false if either we, or any of our parents, are non-save controls
   bool bCanSave	=	getCanSaveParent();
   if(bCanSave)
   {
      Parent::write(stream, tabStop, flags);
   }

}


ConsoleMethod(GuiControl, pointInControl, bool, 4,4,"(int x, int y) Check if point id in the control\n"
              "@param x Point x coordinate in parent coords\n"
              "@param y Point y coordinate in parent coords\n"
              "@return Returns true if the point is in the control, false otherwise")
{
   Point2I kPoint(dAtoi(argv[2]), dAtoi(argv[3]));
   return object->pointInControl(kPoint);
}


// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- //
ConsoleMethod( GuiControl, addGuiControl, void, 3, 3, "(int controlId) Adds the gui control\n"
              "@param controlId integer ID of the control to add\n"
              "@return No Return value")
{

   GuiControl *ctrl = dynamic_cast<GuiControl *>(Sim::findObject(argv[2]));
   if(ctrl)
   {
      object->addObject(ctrl);
   }

}

//-----------------------------------------------------------------------------
//	Make Sure Child 1 is Ordered Just Under Child 2.
//-----------------------------------------------------------------------------
ConsoleMethod(GuiControl, reorderChild, void, 4,4," (child1, child2) uses simset reorder to push child 1 after child 2 - both must already be child controls of this control")
{
   GuiControl* pControl = dynamic_cast<GuiControl*>(Sim::findObject(dAtoi(argv[2])));
   GuiControl* pTarget	 = dynamic_cast<GuiControl*>(Sim::findObject(dAtoi(argv[3])));

   if(pControl && pTarget)
   {
      object->reOrder(pControl,pTarget);
   }
}

ConsoleMethod( GuiControl, getParent, S32, 2, 2, "() @return Returns the Id of the parent control")
{

   GuiControl* pParent		= object->getParent();
   if(pParent)
   {
      return pParent->getId();
   }

   return 0;

}

ConsoleMethod(GuiControl, getRoot, S32, 2, 2, "() @return Returns the Id of the root GuiCanvas control")
{
	GuiCanvas* canvas = object->getRoot();
	if (canvas)
	{
		return canvas->getId();
	}

	return 0;
}

ConsoleMethod( GuiControl, setValue, void, 3, 3, "( value ) Use the setValue method to set the control specific value to value. Purpose and type varies by control type.\n"
                                                                "@param value Some control specific value.\n"
                                                                "@return No return value")
{
   object->setScriptValue(argv[2]);
}

ConsoleMethod( GuiControl, getValue, const char*, 2, 2, "() Use the getValue method to get the control-specific 'value' for this control.\n"
                                                                "@return Returns a control-specific specific value. Varies by control")
{
   return object->getScriptValue();
}

ConsoleMethod( GuiControl, setActive, void, 3, 3, "( isActive ) Use the setActive method to (de)activate this control. Once active, a control can accept inputs. Controls automatically re-shade/skin themselves to reflect their active/inactive state.\n"
                                                                "@param isActive A boolean value. f isActive is true, this control is activated, else it is set to inactive.\n"
                                                                "@return No return value")
{
   object->setActive(dAtob(argv[2]));
}

ConsoleMethod( GuiControl, isActive, bool, 2, 2, "() Use the isActive method to determine if this control is active.\n"
                                                                "An inactive control may visible, but will not accept inputs. It will also normally re-shade or re-skin itself to reflect its inactive state\n"
                                                                "@return Returns true if this control is active.")
{
   return object->isActive();
}

ConsoleMethod( GuiControl, setVisible, void, 3, 3, "( isVisible ) Use the setVisible method to (un)hide this control.\n"
                                                                "@param isVisible A boolean value. If true, the control will be made visible, otherwise the control will be hidden.\n"
                                                                "@return No return value")
{
   object->setVisible(dAtob(argv[2]));
}

ConsoleMethod( GuiControl, makeFirstResponder, void, 3, 3, "( isFirst ) Use the makeFirstResponder method to force this control to become the first responder.\n"
                                                                "@param isFirst A boolean value. If true, then this control become first reponder and at captures inputs before all other controls, excluding dialogs above this control.\n"
                                                                "@return No return value")
{
   object->makeFirstResponder(dAtob(argv[2]));
}

ConsoleMethod( GuiControl, isVisible, bool, 2, 2, "() Use the isVisible method to determine if this control is visible.\n"
                                                                "This can return true, even if the entire control covered by another. This merely means that the control will render if not covered\n"
                                                                "@return Returns true if the control is visible.")
{
   return object->isVisible();
}

ConsoleMethod( GuiControl, isAwake, bool, 2, 2, "() Use the isAwake method to determine if this control is awake.\n"
                                                                "@return Returns true if this control is awake and ready to display")
{
   return object->isAwake();
}

ConsoleMethod( GuiControl, setProfile, void, 3, 3, "(GuiControlProfile p) Sets the currently used from for the GuiControl\n"
              "@param p The profile you wish to set the control to use\n"
              "@return No return value")
{
   GuiControlProfile * profile;

   if(Sim::findObject(argv[2], profile))
      object->setControlProfile(profile);
}

ConsoleMethod( GuiControl, resize, void, 6, 6, "(int x, int y, int w, int h) Resizes the control to the given dimensions")
{
   Point2I newPos(dAtoi(argv[2]), dAtoi(argv[3]));
   Point2I newExt(dAtoi(argv[4]), dAtoi(argv[5]));
   object->resize(newPos, newExt);
}

ConsoleMethod( GuiControl, getPosition, const char*, 2, 2, "() @return A string set up as \"<pos.x> <pos.y>\"")
{
   char *retBuffer = Con::getReturnBuffer(64);
   const Point2I &pos = object->getPosition();
   dSprintf(retBuffer, 64, "%d %d", pos.x, pos.y);
   return retBuffer;
}
ConsoleMethod( GuiControl, getCenter, const char*, 2, 2, "() @return Returns center of control, as space seperated ints")
{
   char *retBuffer = Con::getReturnBuffer(64);
   const Point2I pos = object->getPosition();
   const Point2I ext = object->getExtent();
   Point2I center(pos.x + ext.x/2, pos.y + ext.y/2);
   dSprintf(retBuffer, 64, "%d %d", center.x, center.y);
   return retBuffer;
}

ConsoleMethod( GuiControl, setCenter, void, 4, 4, "(int x, int y) Sets control position, by center - coords are local not global\n"
              "@return No Return value.")
{
   const Point2I ext = object->getExtent();
   Point2I newpos(dAtoi(argv[2])-ext.x/2, dAtoi(argv[3])-ext.y/2);
   object->setPosition(newpos);
}

ConsoleMethod( GuiControl, getGlobalCenter, const char*, 2, 2, "@return Returns center of control, as space seperated ints")
{
   char *retBuffer = Con::getReturnBuffer(64);
   const Point2I tl(0,0);
   Point2I pos		 =	object->localToGlobalCoord(tl);
   const Point2I ext = object->getExtent();
   Point2I center(pos.x + ext.x/2, pos.y + ext.y/2);
   dSprintf(retBuffer, 64, "%d %d", center.x, center.y);
   return retBuffer;
}

ConsoleMethod( GuiControl, getGlobalPosition, const char*, 2, 2, "() @return Returns the control's position converted to global coordinates (position as space-separted integers)")
{
   char *retBuffer = Con::getReturnBuffer(64);
   const Point2I pos(0,0);
   Point2I gPos	=	object->localToGlobalCoord(pos);

   dSprintf(retBuffer, 64, "%d %d", gPos.x, gPos.y);
   return retBuffer;
}
ConsoleMethod( GuiControl, setPositionGlobal, void, 4, 4, "(int x, int y) Sets the control's position in global space\n"
              "@return No return value")
{
   //see if we can turn the x/y into ints directly, 
   Point2I gPos(dAtoi(argv[2]), dAtoi(argv[3]));
   Point2I lPosOffset	=	object->globalToLocalCoord(gPos);
   lPosOffset.x += object->mBounds.point.x;
   lPosOffset.y += object->mBounds.point.y;

   object->mBounds.set(lPosOffset,object->mBounds.extent);
}

ConsoleMethod( GuiControl, setPosition, void, 4, 4, "(int x, int y) Sets the current control position in local space\n"
              "@return No Return Value.")
{
   //see if we can turn the x/y into ints directly, 
   Point2I lPos(dAtoi(argv[2]), dAtoi(argv[3]));
   object->mBounds.set(lPos,object->mBounds.extent);
}

ConsoleMethod( GuiControl, getExtent, const char*, 2, 2, "Get the width and height of the control.\n"
              "@return The height and width as a string with space-separated integers")
{
   char *retBuffer = Con::getReturnBuffer(64);
   const Point2I &ext = object->getExtent();
   dSprintf(retBuffer, 64, "%d %d", ext.x, ext.y);
   return retBuffer;
}

ConsoleMethod( GuiControl, setExtent, void, 4, 4, "(int width, int height) Sets the width & height of the control.\n"
              "@return No Return Value.")
{
   Point2I kExt(dAtoi(argv[2]), dAtoi(argv[3]));
   object->setExtent(kExt);
}

ConsoleMethod( GuiControl, getMinExtent, const char*, 2, 2, "() Get the minimum allowed size of the control.\n"
              "@return Returns the minimum extent as a string with space separated point values <width> <height>")
{
   char *retBuffer = Con::getReturnBuffer(64);
   const Point2I &minExt = object->getMinExtent();
   dSprintf(retBuffer, 64, "%d %d", minExt.x, minExt.y);
   return retBuffer;
}

void GuiControl::onRemove()
{
   // Only invoke script callbacks if they can be received
   if( isMethod("onRemove") )
      Con::executef(this, 1, "onRemove");

   clearFirstResponder();

   Parent::onRemove();

  // If we are a child, notify our parent that we've been removed
  GuiControl *parent = getParent();
  if( parent )
     parent->onChildRemoved( this );
}

void GuiControl::onChildRemoved( GuiControl *child )
{
   // Base does nothing with this
}

// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- //

const char *GuiControl::getScriptValue()
{
   return NULL;
}

void GuiControl::setScriptValue(const char *value)
{
}

void GuiControl::setConsoleVariable(const char *variable)
{
   if (variable)
   {
      mConsoleVariable = StringTable->insert(variable);
   }
   else
   {
      mConsoleVariable = StringTable->EmptyString;
   }
}
  
//-----------------------------------------------------------------------------
//	finds and returns the first immediate child of ours whose
//	internal name matches the passed String. returns Null if not found.
//-----------------------------------------------------------------------------
void GuiControl::setConsoleCommand(const char *newCmd)
{
   if (newCmd)
      mConsoleCommand = StringTable->insert(newCmd);
   else
      mConsoleCommand = StringTable->EmptyString;
}

const char * GuiControl::getConsoleCommand()
{
    return mConsoleCommand;
}

void GuiControl::setSizing(S32 horz, S32 vert)
{
    mHorizSizing = horz;
    mVertSizing = vert;
}


void GuiControl::setVariable(const char *value)
{
   if (mConsoleVariable[0])
      Con::setVariable(mConsoleVariable, value);
}

void GuiControl::setIntVariable(S32 value)
{
   if (mConsoleVariable[0])
      Con::setIntVariable(mConsoleVariable, value);
}

void GuiControl::setFloatVariable(F32 value)
{
   if (mConsoleVariable[0])
      Con::setFloatVariable(mConsoleVariable, value);
}

const char * GuiControl::getVariable()
{
   if (mConsoleVariable[0])
      return Con::getVariable(mConsoleVariable);
   else return NULL;
}

S32 GuiControl::getIntVariable()
{
   if (mConsoleVariable[0])
      return Con::getIntVariable(mConsoleVariable);
   else return 0;
}

F32 GuiControl::getFloatVariable()
{
   if (mConsoleVariable[0])
      return Con::getFloatVariable(mConsoleVariable);
   else return 0.0f;
}

// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- //

bool GuiControl::cursorInControl()
{
   GuiCanvas *root = getRoot();
   if (! root) return false;

   Point2I pt = root->getCursorPos();
   Point2I offset = localToGlobalCoord(Point2I(0, 0));
   if (pt.x >= offset.x && pt.y >= offset.y &&
      pt.x < offset.x + mBounds.extent.x && pt.y < offset.y + mBounds.extent.y)
   {
      return true;
   }
   else
   {
      return false;
   }
}

bool GuiControl::pointInControl(const Point2I& parentCoordPoint)
{
   S32 xt = parentCoordPoint.x - mBounds.point.x;
   S32 yt = parentCoordPoint.y - mBounds.point.y;
   return xt >= 0 && yt >= 0 && xt < mBounds.extent.x && yt < mBounds.extent.y;
}

ConsoleMethod( GuiControl, findHitControl, S32, 4, 4, "(int x, int y) Searches for the control at the given point\n" 
              "@return Returns the Id of the control at the point")
{
   Point2I pos(dAtoi(argv[2]), dAtoi(argv[3]));
   GuiControl *hit = object->findHitControl(pos);
   return hit ? hit->getId() : 0;
}

GuiControl* GuiControl::findHitControl(const Point2I &pt, S32 initialLayer)
{
   iterator i = end(); // find in z order (last to first)
   while (i != begin())
   {
      i--;
      GuiControl *ctrl = static_cast<GuiControl *>(*i);
      if (initialLayer >= 0 && ctrl->mLayer > initialLayer)
      {
         continue;
      }
      else if (ctrl->mVisible && ctrl->pointInControl(pt))
      {
         Point2I ptemp = pt - ctrl->mBounds.point;
         GuiControl *hitCtrl = ctrl->findHitControl(ptemp);

         if(hitCtrl->mProfile->mModal)
            return hitCtrl;
      }
   }
   return this;
}

// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- //

bool GuiControl::isMouseLocked()
{
   GuiCanvas *root = getRoot();
   return root ? root->getMouseLockedControl() == this : false;
}

void GuiControl::mouseLock(GuiControl *lockingControl)
{
   GuiCanvas *root = getRoot();
   if (root)
      root->mouseLock(lockingControl);
}

void GuiControl::mouseLock()
{
   GuiCanvas *root = getRoot();
   if (root)
      root->mouseLock(this);
}

void GuiControl::mouseUnlock()
{
   GuiCanvas *root = getRoot();
   if (root)
      root->mouseUnlock(this);
}

bool GuiControl::onInputEvent(const InputEvent &event)
{
    // Do nothing by default...
   return( false );
}

void GuiControl::onMouseUp(const GuiEvent &event)
{
}

void GuiControl::onMouseDown(const GuiEvent &event)
{
}

void GuiControl::onMouseMove(const GuiEvent &event)
{
   //if this control is a dead end, make sure the event stops here
   if ( !mVisible || !mAwake )
      return;

   //pass the event to the parent
   GuiControl *parent = getParent();
   if ( parent )
      parent->onMouseMove( event );
}

void GuiControl::onMouseDragged(const GuiEvent &event)
{
}

void GuiControl::onMouseEnter(const GuiEvent &)
{
}

void GuiControl::onMouseLeave(const GuiEvent &)
{
}

bool GuiControl::onMouseWheelUp( const GuiEvent &event )
{
   //if this control is a dead end, make sure the event stops here
   if ( !mVisible || !mAwake )
      return true;

   //pass the event to the parent
   GuiControl *parent = getParent();
   if ( parent )
      return parent->onMouseWheelUp( event );
   else
      return false;
}

bool GuiControl::onMouseWheelDown( const GuiEvent &event )
{
   //if this control is a dead end, make sure the event stops here
   if ( !mVisible || !mAwake )
      return true;

   //pass the event to the parent
   GuiControl *parent = getParent();
   if ( parent )
      return parent->onMouseWheelDown( event );
   else
      return false;
}

void GuiControl::onRightMouseDown(const GuiEvent &)
{
}

void GuiControl::onRightMouseUp(const GuiEvent &)
{
}

void GuiControl::onRightMouseDragged(const GuiEvent &)
{
}

void GuiControl::onMiddleMouseDown(const GuiEvent &)
{
}

void GuiControl::onMiddleMouseUp(const GuiEvent &)
{
}

void GuiControl::onMiddleMouseDragged(const GuiEvent &)
{
}


GuiControl* GuiControl::findFirstTabable()
{
   GuiControl *tabCtrl = NULL;
   iterator i;
   for (i = begin(); i != end(); i++)
   {
      GuiControl *ctrl = static_cast<GuiControl *>(*i);
      tabCtrl = ctrl->findFirstTabable();
      if (tabCtrl)
      {
         mFirstResponder = tabCtrl;
         return tabCtrl;
      }
   }

   //nothing was found, therefore, see if this ctrl is tabable
   return ( mProfile != NULL ) ? ( ( mProfile->mTabable && mAwake && mVisible ) ? this : NULL ) : NULL;
}

GuiControl* GuiControl::findLastTabable(bool firstCall)
{
   //if this is the first call, clear the global
   if (firstCall)
      smPrevResponder = NULL;

   //if this control is tabable, set the global
   if (mProfile->mTabable)
      smPrevResponder = this;

   iterator i;
   for (i = begin(); i != end(); i++)
   {
      GuiControl *ctrl = static_cast<GuiControl *>(*i);
      ctrl->findLastTabable(false);
   }

   //after the entire tree has been traversed, return the last responder found
   mFirstResponder = smPrevResponder;
   return smPrevResponder;
}

GuiControl *GuiControl::findNextTabable(GuiControl *curResponder, bool firstCall)
{
   //if this is the first call, clear the global
   if (firstCall)
      smCurResponder = NULL;

   //first find the current responder
   if (curResponder == this)
      smCurResponder = this;

   //if the first responder has been found, return the very next *tabable* control
   else if ( smCurResponder && mProfile->mTabable && mAwake && mVisible && mActive )
      return( this );

   //loop through, checking each child to see if it is the one that follows the firstResponder
   GuiControl *tabCtrl = NULL;
   iterator i;
   for (i = begin(); i != end(); i++)
   {
      GuiControl *ctrl = static_cast<GuiControl *>(*i);
      tabCtrl = ctrl->findNextTabable(curResponder, false);
      if (tabCtrl) break;
   }
   mFirstResponder = tabCtrl;
   return tabCtrl;
}

GuiControl *GuiControl::findPrevTabable(GuiControl *curResponder, bool firstCall)
{
   if (firstCall)
      smPrevResponder = NULL;

   //if this is the current reponder, return the previous one
   if (curResponder == this)
      return smPrevResponder;

   //else if this is a responder, store it in case the next found is the current responder
   else if ( mProfile->mTabable && mAwake && mVisible && mActive )
      smPrevResponder = this;

   //loop through, checking each child to see if it is the one that follows the firstResponder
   GuiControl *tabCtrl = NULL;
   iterator i;
   for (i = begin(); i != end(); i++)
   {
      GuiControl *ctrl = static_cast<GuiControl *>(*i);
      tabCtrl = ctrl->findPrevTabable(curResponder, false);
      if (tabCtrl) break;
   }
   mFirstResponder = tabCtrl;
   return tabCtrl;
}

void GuiControl::onLoseFirstResponder()
{
    // Since many controls have visual cues when they are the firstResponder...
    setUpdate();
}

bool GuiControl::ControlIsChild(GuiControl *child)
{
   //function returns true if this control, or one of it's children is the child control
   if (child == this)
      return true;

   //loop through, checking each child to see if it is ,or contains, the firstResponder
   iterator i;
   for (i = begin(); i != end(); i++)
   {
      GuiControl *ctrl = static_cast<GuiControl *>(*i);
      if (ctrl->ControlIsChild(child)) return true;
   }

   //not found, therefore false
   return false;
}

bool GuiControl::isFirstResponder()
{
   GuiCanvas *root = getRoot();
   return root && root->getFirstResponder() == this;
}

void GuiControl::setFirstResponder( GuiControl* firstResponder )
{
   if (firstResponder && firstResponder->mProfile->mCanKeyFocus)
   {
      mFirstResponder = firstResponder;
   }
   else if (!firstResponder)
   {
      mFirstResponder = NULL;
   }

   GuiControl *parent = getParent();
   if ( parent )
      parent->setFirstResponder( firstResponder );
}

void GuiControl::setFirstResponder()
{
    if ( mAwake && mVisible )
    {
       GuiControl *parent = getParent();
       if (mProfile->mCanKeyFocus == true && parent != NULL )
      {
         parent->setFirstResponder(this);

         // Since many controls have visual cues when they are the firstResponder...
         this->setUpdate();	
      }
    }
}

ConsoleMethod(GuiControl, setFirstResponder, void , 2, 2, "Sets this control as the first responder")
{
   object->setFirstResponder();
}

void GuiControl::clearFirstResponder()
{
   GuiControl *parent = this;
   while((parent = parent->getParent()) != NULL)
   {
      if(parent->mFirstResponder == this)
         parent->mFirstResponder = NULL;
      else
         break;
   }
}

// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- //

void GuiControl::buildAcceleratorMap()
{
   //add my own accel key
   addAcceleratorKey();

   //add all my childrens keys
   iterator i;
   for(i = begin(); i != end(); i++)
   {
      GuiControl *ctrl = static_cast<GuiControl *>(*i);
      ctrl->buildAcceleratorMap();
   }
}

void GuiControl::addAcceleratorKey()
{
   //see if we have an accelerator
   if (mAcceleratorKey == StringTable->EmptyString)
      return;

   EventDescriptor accelEvent;
   ActionMap::createEventDescriptor(mAcceleratorKey, &accelEvent);

   //now we have a modifier, and a key, add them to the canvas
   GuiCanvas *root = getRoot();
   if (root)
      root->addAcceleratorKey(this, 0, accelEvent.eventCode, accelEvent.flags);
}

void GuiControl::acceleratorKeyPress(U32 index)
{
   onAction();
}

void GuiControl::acceleratorKeyRelease(U32 index)
{
   //do nothing
}

bool GuiControl::onKeyDown(const GuiEvent &event)
{
   //pass the event to the parent
   GuiControl *parent = getParent();
   if (parent)
      return parent->onKeyDown(event);
   else
      return false;
}

bool GuiControl::onKeyRepeat(const GuiEvent &event)
{
   // default to just another key down.
   return onKeyDown(event);
}

bool GuiControl::onKeyUp(const GuiEvent &event)
{
   //pass the event to the parent
   GuiControl *parent = getParent();
   if (parent)
      return parent->onKeyUp(event);
   else
      return false;
}

// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- //

void GuiControl::onAction()
{
   if (! mActive)
      return;

   //execute the console command
   if (mConsoleCommand && mConsoleCommand[0])
   {
      execConsoleCallback();
   }
   else
      Con::executef(this, 1, "onAction");
}

void GuiControl::onMessage(GuiControl *sender, S32 msg)
{
}

void GuiControl::messageSiblings(S32 message)
{
   GuiControl *parent = getParent();
   if (! parent) return;
   GuiControl::iterator i;
   for(i = parent->begin(); i != parent->end(); i++)
   {
      GuiControl *ctrl = dynamic_cast<GuiControl *>(*i);
      if (ctrl != this)
         ctrl->onMessage(this, message);
   }
}

// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- //

void GuiControl::onDialogPush()
{
   // Notify Script.
   if( isMethod("onDialogPush") )
      Con::executef(this, 1, "onDialogPush");

}

void GuiControl::onDialogPop()
{
   // Notify Script.
   if( isMethod("onDialogPop") )
      Con::executef(this, 1, "onDialogPop");
}

//------------------------------------------------------------------------------
void GuiControl::setVisible(bool value)
{
    mVisible = value;
   iterator i;
   setUpdate();
   for(i = begin(); i != end(); i++)
   {
      GuiControl *ctrl = static_cast<GuiControl *>(*i);
      ctrl->clearFirstResponder();
    }

    GuiControl *parent = getParent();
    if (parent)
       parent->childResized(this);
}


void GuiControl::makeFirstResponder(bool value)
{
   if ( value )
      //setFirstResponder(this);
      setFirstResponder();
   else
      clearFirstResponder();
}

void GuiControl::setActive( bool value )
{
   mActive = value;

   if ( !mActive )
      clearFirstResponder();

   if ( mVisible && mAwake )
      setUpdate();
}

void GuiControl::getScrollLineSizes(U32 *rowHeight, U32 *columnWidth)
{
    // default to 10 pixels in y, 30 pixels in x
    *columnWidth = 30;
    *rowHeight = 30;
}

void GuiControl::renderJustifiedText(Point2I offset, Point2I extent, const char *text)
{
   GFont *font = mProfile->mFont;
   S32 textWidth = font->getStrWidth((const UTF8*)text);
   Point2I start;

   // align the horizontal
   switch( mProfile->mAlignment )
   {
      case GuiControlProfile::RightJustify:
         start.set( extent.x - textWidth, 0 );
         break;
      case GuiControlProfile::CenterJustify:
         start.set( ( extent.x - textWidth) / 2, 0 );
         break;
      default:
         // GuiControlProfile::LeftJustify
         start.set( 0, 0 );
         break;
   }

   // If the text is longer then the box size, (it'll get clipped) so
   // force Left Justify

   if( textWidth > extent.x )
      start.set( 0, 0 );

   // center the vertical
   if(font->getHeight() > (U32)extent.y)
      start.y = 0 - ((font->getHeight() - extent.y) / 2) ;
   else
      start.y = ( extent.y - font->getHeight() ) / 2;

   dglDrawText( font, start + offset, text, mProfile->mFontColors );
}

void GuiControl::getCursor(GuiCursor *&cursor, bool &showCursor, const GuiEvent &lastGuiEvent)
{
   if(GuiControl::smCursorChanged != -1 && !isMouseLocked())
   {
      // We've already changed the cursor, 
      // so set it back before we change it again.
      Input::popCursor();

      // We haven't changed it
      GuiControl::smCursorChanged = -1;
   }
}

ConsoleMethod(GuiControl, getScriptFile, const char*, 2, 2, "GuiControl.getScriptFile() - gets the script file the control was created in\n"
              "@return String containing script file path")
{
   return object->getScriptFile();
}

#ifdef TORQUE_TOOLS
ConsoleMethod(GuiControl, setScriptFile, void, 3, 3, "(string filepath) GuiControl.setScriptFile() - sets the script file the control was created in"
              "@param The path to the scriptfile to set"
              "@return No Return Value")
{
   object->setScriptFile(argv[2]);
}
#endif

StringTableEntry GuiControl::getScriptFile()
{
   return mScriptFile;
}

const char* GuiControl::execConsoleCallback()
{
   if (mConsoleCommand && mConsoleCommand[0])
   {
      Con::setVariable("$ThisControl", avar("%d",getId()));
      return Con::evaluate(mConsoleCommand, false);
   }
   return "";
}

const char* GuiControl::execAltConsoleCallback()
{
   if(mAltConsoleCommand && mAltConsoleCommand[0])
   {
      Con::setVariable("$ThisControl", avar("%d",getId()));
      return Con::evaluate(mAltConsoleCommand, false);
   }
   return "";
}
