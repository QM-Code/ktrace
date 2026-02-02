from karma import config, webhttp


def render_lightbox():
    preview_alt = webhttp.html_escape(config.ui_text("labels.screenshot_preview_alt"))
    close_label = webhttp.html_escape(config.ui_text("labels.lightbox_close"))
    return f"""<div class="lightbox" id="lightbox" hidden>
  <div class="lightbox-content">
    <button class="lightbox-close" type="button" aria-label="{close_label}">×</button>
    <img id="lightbox-image" src="" alt="{preview_alt}">
  </div>
</div>"""


def render_lightbox_script():
    return """<script>
  (function () {
    const lightbox = document.getElementById("lightbox");
    const image = document.getElementById("lightbox-image");
    const closeBtn = lightbox.querySelector(".lightbox-close");
    function openLightbox(src) {
      image.src = src;
      lightbox.hidden = false;
      lightbox.classList.add("open");
      closeBtn.focus();
    }
    function closeLightbox() {
      closeBtn.blur();
      lightbox.classList.remove("open");
      lightbox.hidden = true;
      image.src = "";
    }
    document.addEventListener("click", (event) => {
      const thumb = event.target.closest(".thumb[data-full]");
      if (!thumb) return;
      const src = thumb.getAttribute("data-full") || thumb.src;
      openLightbox(src);
    });
    closeBtn.addEventListener("click", closeLightbox);
    lightbox.addEventListener("click", (event) => {
      if (event.target === lightbox) {
        closeLightbox();
      }
    });
    document.addEventListener("keydown", (event) => {
      if (event.key === "Escape") {
        closeLightbox();
      }
    });
  })();
</script>"""
