/*!
 * freetype2.js
 *
 * auxiliary JavaScript functions for freetype.org
 *
 * written 2012 by Werner Lemberg, updated 2021 by Anurag Thakur
 */

// Add a bottom bar if the document length exceeds the window height.
// Additionally ensure that the TOC background reaches the bottom of the
// window.

function addBottomBar() {
  const columnHeight = document.querySelector(".colright").clientHeight;
  const topBarHeight = document.querySelector("#top").clientHeight;
  const winHeight = window.innerHeight;

  if (columnHeight + topBarHeight > winHeight) {
    document.getElementById("TOC-bottom").style.height = columnHeight + "px";

    /* add bottom bar if it doesn't exist already */
    if (!document.querySelector("#bottom")) {
      document.body.insertAdjacentHTML(
        "beforeend",
        '<div class="bar" id="bottom"></div>'
      );
    }
  } else {
    let tocBHeight = winHeight - topBarHeight;
    document.getElementById("TOC-bottom").style.height = tocBHeight + "px";
    document.getElementById("bottom").remove();
  }
}

// `Slightly' scroll the TOC so that its top moves up to the top of the
// screen if we scroll down.  The function also assures that the bottom of
// the TOC doesn't cover the bottom bar (e.g., if the window height is very
// small).
function adjustTOCPosition() {
  const docHeight = document.body.scrollHeight;
  const TOCHeight = document.querySelector("#TOC").clientHeight;
  const bottomBarHeight = document.querySelector("#bottom").clientHeight;
  const topBarHeight = document.querySelector("#top").clientHeight;

  const scrollTopPos = window.scrollY;
  const topBarBottomPos = 0 + topBarHeight;
  const bottomBarTopPos = docHeight - bottomBarHeight;

  if (scrollTopPos >= topBarBottomPos) {
    document.getElementById("TOC").style.top = "0px";
  }
  if (scrollTopPos < topBarBottomPos) {
    let topPos = topBarBottomPos - scrollTopPos;
    document.getElementById("TOC").style.top = topPos + "px";
  }
  if (bottomBarTopPos - scrollTopPos < TOCHeight) {
    let topPos = bottomBarTopPos - scrollTopPos - TOCHeight;
    document.getElementById("TOC").style.top = topPos + "px";
  }
}

// Hook functions into loading, resizing, and scrolling events.
window.onload = function () {
  addBottomBar();
  adjustTOCPosition();

  window.onscroll = function () {
    adjustTOCPosition();
  };

  window.onresize = function () {
    addBottomBar();
  };
};

/* end of freetype2.js */
