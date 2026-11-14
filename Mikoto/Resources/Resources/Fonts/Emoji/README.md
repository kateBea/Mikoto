# Noto Color Emoji

Bundled fallback for colour emoji in MSDF and Slug text. Source: [googlefonts/noto-emoji v2.051](https://github.com/googlefonts/noto-emoji/tree/8998f5dd683424a73e2314a8c1f1e359c19e8742/fonts).

`NotoColorEmoji.ttf` is licensed under the SIL Open Font License; see `LICENSE.txt`. Keep both files when redistributing engine resources.

Mikoto shapes supported emoji clusters with HarfBuzz and rasterizes CBDT/CBLC or COLR v0 colour glyphs with FreeType. COLR v1 paint graphs and SVG glyphs are not implemented. Emoji coverage and joined sequences depend on the fallback font. Ordinary text continues to use the selected font's MSDF or Slug outlines; general complex-script shaping is separate work.

## Debug layer

Set `text_rendering = true` in `[debug]` in `app-config.toml`. F10 hides/shows the window. Edit `Resources/Text samples/Unicode and emoji.txt` as UTF-8 and click **Reload sample**. The independent debug scene is not saved into your project. Controls compare fonts, repeated passages, scroll position, opacity and outlines. The renderer retains its 65,536-glyph frame limit; very large repeat counts intentionally exercise that limit.

Text component outlines use 0–0.125 em of outside stroke, defaulting to disabled. Both backends use the MTSDF RGB distance for thin strokes to preserve corners, blending to the true-distance alpha channel for wider rounded strokes. Slug retains analytic fill and samples that companion atlas only for the optional stroke. Colour emoji bypass outlines and fill RGB tint, but respect text opacity. General complex-script shaping and the ImGui text editor's joined-emoji display are not implemented by this renderer integration.
