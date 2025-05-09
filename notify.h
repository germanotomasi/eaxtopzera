#pragma once

// modelled after the original valve 'developer 1' debug console
// https://github.com/LestaD/SourceEngine2007/blob/master/se2007/engine/console.cpp

class NotifyText {
public:
	std::string m_text;
	Color		m_color;
	float		m_time;
	float value_thing;

public:
	__forceinline NotifyText(const std::string& text, Color color, float time) : m_text{ text }, m_color{ color }, m_time{ time }, value_thing{ 0.f } {}
};

class Notify {
private:
	std::vector< std::shared_ptr< NotifyText > > m_notify_text;

public:
	Color gui{ 255,255,255,255 };
	__forceinline Notify() : m_notify_text{} {}

	__forceinline void add(const std::string& text, Color color = colors::white, float time = 8.f, bool console = true) {
		// modelled after 'CConPanel::AddToNotify'
		m_notify_text.push_back(std::make_shared< NotifyText >(text, color, time));

		if (console)
			g_cl.print(text);
	}

	// modelled after 'CConPanel::DrawNotify' and 'CConPanel::ShouldDraw'
	void think() {
		int		x{ 8 }, y{ 5 }, size{ render::menu.m_size.m_height + 3 };
		Color	color;
		float	left;

		// update lifetimes.
		for (size_t i{}; i < m_notify_text.size(); ++i) {
			auto notify = m_notify_text[i];

			notify->m_time -= g_csgo.m_globals->m_frametime;

			if (notify->value_thing <= 0.f && notify->m_time <= 1.f) {
				m_notify_text.erase(m_notify_text.begin() + i);
				continue;
			}
		}

		// we have nothing to draw.
		if (m_notify_text.empty())
			return;

		// iterate entries.
		for (size_t i{}; i < m_notify_text.size(); ++i) {



			auto notify = m_notify_text[i];

			left = notify->m_time;
			color = notify->m_color;
			auto value_thing = 1.f;

			if (left < 8.f && left > 0.f && notify->value_thing < 1.f)
				notify->value_thing += g_csgo.m_globals->m_frametime * 4.f;
			else if (left <= 0.f && notify->value_thing > 0.f)
				notify->value_thing -= g_csgo.m_globals->m_frametime * 4.f;

			math::clamp(notify->value_thing, 0.f, 1.f);


			/*
			if (left <= 8.f && left > 7.75f) {
				float f = 0.f;
				f = abs(left - 8.0) / 0.250;
				value_thing = f * 1;

	

			}

			if (left <= .25f + 0.125f) {
				float f = left;
				float f2 = left;
				math::clamp(f, 0.f, .25f + 0.125f);
				math::clamp(f2, 0.f, .125f);

				f /= .25f + 0.125f;
				f2 /= .125f;

				value_thing = f * 1;

				if (left <= 0.125f)
					color.a() = (int)(f2 * 255.f);

				if (i == 0 && f < 0.125f)
					y -= std::clamp(size * (0.125f - f2 / 0.125f), 0.f, 999999999.f);
			}

			else*/
				color.a() = 255 * notify->value_thing;

			Color troll = gui;

			troll.a() = color.a();

			render::menu.semi_filled_text_noshadow(x, y, color, notify->m_text, render::ALIGN_LEFT, notify->value_thing);
			y += size;
		}
	}
};

extern Notify g_notify;