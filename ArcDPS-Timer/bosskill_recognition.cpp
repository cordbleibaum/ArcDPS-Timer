#include "bosskill_recognition.h"
#include "util.h"

BossKillRecognition::BossKillRecognition(GW2MumbleLink& mumble_link, Settings& settings)
:	mumble_link(mumble_link),
	settings(settings) {
	add_defaults();
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
				for (auto& condition_list : conditions) {
					bool is_true = true;
					for (auto& condition : condition_list) {
						is_true &= condition(data);
					}

					if (is_true) {
						log_debug("timer: stopping on boss kill condition list");
						bosskill_signal(ev->time);
					}
				}

				bool is_kill = std::find(std::begin(settings.additional_boss_ids), std::end(settings.additional_boss_ids), log_species_id) != std::end(settings.additional_boss_ids);
				for (const auto& agent_species_id : data.log_agents) {
					is_kill |= std::find(std::begin(settings.additional_boss_ids), std::end(settings.additional_boss_ids), agent_species_id) != std::end(settings.additional_boss_ids);
				}

				data.log_agents.clear();

				if (is_kill) {
					log_debug("timer: stopping on boss kill additional ID");
					bosskill_signal(ev->time);
				}
			}
		}
		else if (ev->is_statechange == cbtstatechange::CBTS_LOGSTART) {
			log_start_time = std::chrono::system_clock::now() - std::chrono::milliseconds{ timeGetTime() - ev->time };
		}
	}
}

void BossKillRecognition::add_defaults(){
	// Fractals
	emplace_conditions({ condition_boss_id(11265) }); // Swampland - Bloomhunger
	emplace_conditions({ condition_boss_id(11239) }); // Underground Facility - Dredge
	emplace_conditions({ condition_boss_id(11240) }); // Underground Facility - Elemental
	emplace_conditions({ condition_boss_id(11485) }); // Volcanic - Imbued Shaman
	emplace_conditions({ condition_boss_id(11296) }); // Cliffside - Archdiviner
	emplace_conditions({ condition_boss_id(19697) }); // Mai Trin Boss Fractal - Mai Trin
	emplace_conditions({ condition_boss_id(12906) }); // Thaumanova - Thaumanova Anomaly
	emplace_conditions({ condition_boss_id(11333) }); // Snowblind - Shaman
	emplace_conditions({ condition_boss_id(11402) }); // Aquatic Ruins - Jellyfish Beast
	emplace_conditions({ condition_boss_id(16617) }); // Chaos - Gladiator
	emplace_conditions({ condition_boss_id(20497) }); // Deepstone - The Voice
	emplace_conditions({ condition_boss_id(12900) }); // Molten Furnace - Engineer
	emplace_conditions({ condition_boss_id(17051) }); // Nightmare - Ensolyss
	emplace_conditions({ condition_boss_id(16948) }); // Nightmare CM - Ensolyss
	emplace_conditions({ condition_boss_id(17830) }); // Shattered Observatory - Arkk
	emplace_conditions({ condition_boss_id(17759) }); // Shattered Observatory - Arkk CM
	emplace_conditions({ condition_boss_id(11408) }); // Urban Battleground - Captain Ashym
	emplace_conditions({ condition_boss_id(19664) }); // Twilight Oasis - Amala
	emplace_conditions({ condition_boss_id(21421) }); // Sirens Reef - Captain Crowe
	emplace_conditions({ condition_boss_id(11328) }); // Uncategorized - Asura
	emplace_conditions({ condition_boss_id(12898) }); // Molten Boss - Berserker
	emplace_conditions({ condition_boss_id(12267) }); // Aetherblade - Frizz

	// Raids
	emplace_conditions({ condition_boss_id(15375) }); // Sabetha
	emplace_conditions({ condition_boss_id(16115) }); // Matthias Gabrel
	emplace_conditions({ condition_boss_id(17154) }); // Deimos
	emplace_conditions({ condition_boss_id(19450) }); // Dhuum,
	emplace_conditions({ condition_boss_id(21041) }); // Qadim
	emplace_conditions({ condition_boss_id(22000) }); // Qadim the Peerless
	emplace_conditions({ condition_boss_id(16246) }); // Xera
	emplace_conditions({ condition_boss_id(16286) }); // Xera

	// Dungeons
	emplace_conditions({ condition_boss_id(7018) });  // AC P1
	emplace_conditions({ condition_boss_id(9077) });  // AC P2
	emplace_conditions({ condition_boss_id(9078) });  // AC P3
	emplace_conditions({ condition_boss_id(10580) }); // Arah P2
	emplace_conditions({ condition_boss_id(10218) }); // CoE P1
	emplace_conditions({ condition_boss_id(10337) }); // CoE P2
	emplace_conditions({ condition_boss_id(10404) }); // CoE P3
}

void BossKillRecognition::emplace_conditions(std::initializer_list<std::function<bool(EncounterData&)>> initializer) {
	conditions.emplace_back(initializer);
}

std::function<bool(EncounterData&)> condition_boss_id(uintptr_t boss_id) {
	return [&, boss_id](EncounterData& data) {
		if (data.log_species_id == boss_id) {
			return true;
		}

		for (const auto& agent_species_id : data.log_agents) {
			if (agent_species_id == boss_id) {
				return true;
			}
		}

		return false;
	};
}