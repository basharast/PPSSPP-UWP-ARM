#pragma once

#include "ppsspp_config.h"

#include <map>
#include "Common/Render/Text/draw_text.h"

#if PPSSPP_PLATFORM(UWP)

#include <d2d1_3.h>
#include <dwrite_3.h>

struct TextDrawerContext;
// Internal struct but all details in .cpp file (pimpl to avoid pulling in excessive headers here)
class TextDrawerFontContext;

class TextDrawerUWP : public TextDrawer {
public:
	TextDrawerUWP(Draw::DrawContext *draw);
	~TextDrawerUWP();

	uint32_t SetFont(const char *fontName, int size, int flags) override;
	void SetFont(uint32_t fontHandle) override;  // Shortcut once you've set the font once.
	void MeasureString(const char *str, size_t len, float *w, float *h) override;
	void MeasureStringRect(const char *str, size_t len, const Bounds &bounds, float *w, float *h, int align = ALIGN_TOPLEFT) override;
	void DrawString(DrawBuffer &target, const char *str, float x, float y, uint32_t color, int align = ALIGN_TOPLEFT) override;
	void DrawStringBitmap(std::vector<uint8_t> &bitmapData, TextStringEntry &entry, Draw::DataFormat texFormat, const char *str, int align = ALIGN_TOPLEFT) override;
	// Use for housekeeping like throwing out old strings.
	void OncePerFrame() override;

protected:
	void ClearCache() override;
	void RecreateFonts();  // On DPI change

	TextDrawerContext *ctx_;
	std::map<uint32_t, std::unique_ptr<TextDrawerFontContext>> fontMap_;

	uint32_t fontHash_;
	std::map<CacheKey, std::unique_ptr<TextStringEntry>> cache_;
	std::map<CacheKey, std::unique_ptr<TextMeasureEntry>> sizeCache_;
	

	// Direct2D drawing components.
	ID2D1Factory3*        m_d2dFactory2;
	ID2D1Device2*         m_d2dDevice2;
	ID2D1DeviceContext2*  m_d2dContext2;
#if !defined(BUILD14393)
	ID2D1Factory5*        m_d2dFactory;
	ID2D1Device4*         m_d2dDevice;
	ID2D1DeviceContext4*  m_d2dContext;
#endif
	ID2D1SolidColorBrush* m_d2dWhiteBrush;

	// DirectWrite drawing components.

	IDWriteFactory3*        m_dwriteFactory2;
#if !defined(BUILD14393)
	IDWriteFactory5*        m_dwriteFactory;
#endif
	IDWriteFontFile*        m_fontFile;
	IDWriteFontSet*         m_fontSet;

	IDWriteFontSetBuilder* m_fontSetBuilder2;
#if !defined(BUILD14393)
	IDWriteFontSetBuilder1* m_fontSetBuilder;
#endif
	IDWriteFontCollection1* m_fontCollection;

};

#endif
