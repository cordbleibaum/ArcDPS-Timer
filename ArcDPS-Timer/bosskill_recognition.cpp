#include "bosskill_recognition.h"
#include "util.h"

BossKillRecognition::BossKillRecognition(GW2MumbleLink& mumble_link, Settings& settings)
:	mumble_link(mumble_link),
	settings(settings) {
}

void BossKillRecognition::mod_combat(cbtevent* ev, ag* src, ag* dst, const char* skillname, uint64_t id) {
	if (ev) {
		if (src && src->prof > 9) {
			std::scoped_lock<std::mutex> guard(logagents_mutex);
			data.log_agents.insert(src->prof);
		}
		if (dst && dst->prof > 9) { // TODO: make sure minions, pets, etc get excluded
			std::scoped_lock<std::mutex> guard(logagents_mutex);
			data.log_agents.insert(dst->prof);

			if ((!ev->is_buffremove && !ev->is_activation && !ev->is_statechange) || (ev->buff && ev->buff_dmg)) {
				last_damage_ticks = calculate_ticktime(ev->time);
			}
		}

		if (ev->is_statechange == cbtstatechange::CBTS_LOGEND) {
			std::scoped_lock<std::mutex> guard(logagents_mutex);
			uintptr_t log_species_id = ev->src_agent;
			data.log_species_id = ev->src_agent;

			auto log_duration = std::chrono::system_clock::now() - log_start_time;
			if (log_duration > std::chrono::seconds(settings.early_gg_threshold)) {
				std::set<uintptr_t> last_bosses = {
					// Fractals
					11265, // Swampland - Bloomhunger 
					11239, // Underground Facility - Dredge
					11240, // Underground Facility - Elemental
					11485, // Volcanic - Imbued Shaman
					11296, // Cliffside - Archdiviner
					19697, // Mai Trin Boss Fractal - Mai Trin
					12906, // Thaumanova - Thaumanova Anomaly
					11333, // Snowblind - Shaman
					11402, // Aquatic Ruins - Jellyfish Beast
					16617, // Chaos - Gladiator
					20497, // Deepstone - The Voice
					12900, // Molten Furnace - Engineer
					17051, // Nightmare - Ensolyss
					16948, // Nightmare CM - Ensolyss
					17830, // Shattered Observatory - Arkk
					17759, // Shattered Observatory - Arkk CM
					11408, // Urban Battleground - Captain Ashym
					19664, // Twilight Oasis - Amala
					21421, // Sirens Reef - Captain Crowe
					11328, // Uncategorized - Asura
					12898, // Molten Boss - Berserker
					12267, // Aetherblade - Frizz
					// Raids
					15375, // Sabetha
					16115, // Matthias Gabrel
					17154, // Deimos
					19450, // Dhuum,
					21041, // Qadim
					22000, // Qadim the Peerless
					16246, // Xera
					16286, // Xera
					// Dungeons
					7018,  // AC P1
					9077,  // AC P2
					9078,  // AC P3
					10580, // Arah P2
					10218, // CoE P1
					10337, // CoE P2
					10404, // CoE P3
				};

				bool is_bosslog_end = false;
				if (std::find(std::begin(last_bosses), std::end(last_bosses), log_species_id) != std::end(last_bosses)) {
					log_debug("Stopping on integrated ID: " + std::to_string(log_species_id));
					is_bosslog_end = true;
				}
				if (std::find(std::begin(settings.additional_boss_ids), std::end(settings.additional_boss_ids), log_species_id) != std::end(settings.additional_boss_ids)) {
					log_debug("Stopping on additional ID: " + std::to_string(log_species_id));
					is_bosslog_end = true;
				}

				bool is_ooc_end = false;
				for (const auto& agent_species_id : data.log_agents) {
					if (std::find(std::begin(last_bosses), std::end(last_bosses), agent_species_id) != std::end(last_bosses)) {
						log_debug("Stopping on integrated ID: " + std::to_string(agent_species_id));
						is_ooc_end = true;
					}
					if (std::find(std::begin(settings.additional_boss_ids), std::end(settings.additional_boss_ids), agent_species_id) != std::end(settings.additional_boss_ids)) {
						log_debug("Stopping on additional ID: " + std::to_string(agent_species_id));
						is_ooc_end = true;
					}
				}

				data.log_agents.clear();

				if (is_bosslog_end || is_ooc_end) {
					log_debug("timer: stopping on boss kill");
					bosskill_signal(ev->time);
				}
			}
		}
		else if (ev->is_statechange == cbtstatechange::CBTS_LOGSTART) {
			log_start_time = std::chrono::system_clock::now() - std::chrono::milliseconds{ timeGetTime() - ev->time };
		}
	}
}

std::function<bool(EncounterData&)> condition_boss_id(uintptr_t boss_id) {
	return [&, boss_id](EncounterData& data) {
		return true;
	};
}
