#include "bosskill_recognition.h"
#include "util.h"

using namespace std::chrono_literals;

BossKillRecognition::BossKillRecognition(GW2MumbleLink& mumble_link, const Settings& settings)
:	mumble_link(mumble_link),
	settings(settings) {
	add_defaults();
}

void BossKillRecognition::mod_combat(cbtevent* ev, ag* src, ag* dst, const char* skillname, uint64_t id) {
	if (ev) {
		if (src && src->prof > 9) {
			std::scoped_lock<std::mutex> guard(logagents_mutex);
			
			if (data.log_agents.find(src->id) == data.log_agents.cend()) {
				AgentData agent;
				agent.species_id = src->prof;
				data.log_agents[src->id] = agent;
			}

			if (!ev->is_buffremove && !ev->is_activation && !ev->is_statechange && !ev->buff) {
				data.log_agents[src->id].damage_dealt += ev->value;
			}

			if (ev->buff && ev->buff_dmg) {
				data.log_agents[src->id].damage_dealt += ev->buff_dmg;
			}
		}
		if (dst && dst->prof > 9 && ev->iff != IFF_FRIEND) { // TODO: make sure minions, pets, etc get excluded
			std::scoped_lock<std::mutex> guard(logagents_mutex);

			if (data.log_agents.find(dst->id) == data.log_agents.cend()) {
				AgentData agent;
				agent.species_id = dst->prof;
				data.log_agents[dst->id] = agent;
			}

			if ((!ev->is_buffremove && !ev->is_activation && !ev->is_statechange && !ev->buff) || (ev->buff && ev->buff_dmg)) {
				auto time = calculate_ticktime(ev->time);
				if (time > data.log_agents[dst->id].last_hit) {
					data.log_agents[dst->id].last_hit = time;
				}
			}

			if (!ev->is_buffremove && !ev->is_activation && !ev->is_statechange && !ev->buff) {
				data.log_agents[dst->id].damage_taken += ev->value;
			}

			if (ev->buff && ev->buff_dmg) {
				data.log_agents[dst->id].damage_taken += ev->buff_dmg;
			}
		}

		if (ev->is_statechange == cbtstatechange::CBTS_LOGEND) {
			std::scoped_lock<std::mutex> guard(logagents_mutex);
			const uintptr_t log_species_id = ev->src_agent;
			data.log_species_id = ev->src_agent;

			auto log_duration = std::chrono::system_clock::now() - log_start_time;
			if (log_duration > std::chrono::seconds(settings.early_gg_threshold)) {
				for (auto& boss : bosses) {
					bool is_true = true;
					for (auto& condition : boss.conditions) {
						is_true &= condition(data);

						if (!is_true) {
							break;
						}
					}

					if (is_true) {
						log_debug("timer: stopping on boss kill condition list");
						bosskill_signal(boss.timing(data));
					}
				}

				bool is_kill = std::find(std::begin(settings.additional_boss_ids), std::end(settings.additional_boss_ids), log_species_id) != std::end(settings.additional_boss_ids);
				for (const auto &[id, agent] : data.log_agents) {
					is_kill |= std::find(std::begin(settings.additional_boss_ids), std::end(settings.additional_boss_ids), agent.species_id) != std::end(settings.additional_boss_ids);
				}

				if (is_kill) {
					log_debug("timer: stopping on boss kill additional ID");
					bosskill_signal(timing_last_hit_npc()(data));
				}

				data.archive();
			}
		}
		else if (ev->is_statechange == cbtstatechange::CBTS_LOGSTART) {
			log_start_time = std::chrono::system_clock::now() - std::chrono::milliseconds{ timeGetTime() - ev->time };
			data.map_id = mumble_link->getMumbleContext()->mapId;
		}
	}
}

void BossKillRecognition::add_defaults(){
	// Fractals
	emplace_conditions(timing_last_hit_npc(), { condition_npc_id(11265) }); // Swampland - Bloomhunger
	emplace_conditions(timing_last_hit_npc_id(11239), { condition_npc_id(11239) }); // Underground Facility - Dredge
	emplace_conditions(timing_last_hit_npc_id(11240), { condition_npc_id(11240) }); // Underground Facility - Elemental
	emplace_conditions(timing_last_hit_npc(), { condition_npc_damage_taken(11485, 400000) }); // Volcanic - Imbued Shaman
	emplace_conditions(timing_last_hit_npc_id(11296), { condition_npc_damage_taken(11296, 200000) }); // Cliffside - Archdiviner
	emplace_conditions(timing_last_hit_npc(), { condition_npc_id(19697) }); // Mai Trin Boss Fractal - Mai Trin
	emplace_conditions(timing_last_hit_npc(), { condition_npc_id(12906) }); // Thaumanova - Thaumanova Anomaly
	emplace_conditions(timing_last_hit_npc(), { condition_npc_id(11333) }); // Snowblind - Shaman
	emplace_conditions(timing_last_hit_npc(), { condition_npc_id(11402) }); // Aquatic Ruins - Jellyfish Beast
	emplace_conditions(timing_last_hit_npc(), { condition_npc_id(16617) }); // Chaos - Gladiator
	emplace_conditions(timing_last_hit_npc(), { condition_npc_id(20497) }); // Deepstone - The Voice
	emplace_conditions(timing_last_hit_npc(), { condition_npc_id(12900), condition_map_id(955)}); // Molten Furnace - Engineer
	emplace_conditions(timing_last_hit_npc(), { condition_npc_id(17051) }); // Nightmare - Ensolyss
	emplace_conditions(timing_last_hit_npc(), { condition_npc_id(16948) }); // Nightmare CM - Ensolyss
	emplace_conditions(timing_last_hit_npc_id(17830), { condition_npc_id(17830) }); // Shattered Observatory - Arkk
	emplace_conditions(timing_last_hit_npc_id(17759), { condition_npc_id(17759) }); // Shattered Observatory - Arkk CM
	emplace_conditions(timing_last_hit_npc(), { condition_npc_damage_taken(11408, 400000)}); // Urban Battleground - Captain Ashym
	emplace_conditions(timing_last_hit_npc(), { condition_npc_id(19664) }); // Twilight Oasis - Amala
	emplace_conditions(timing_last_hit_npc(), { condition_npc_id(21421) }); // Sirens Reef - Captain Crowe
	emplace_conditions(timing_last_hit_npc(), { condition_npc_damage_dealt(11328, 100) }); // Uncategorized - Asura
	emplace_conditions(timing_last_hit_npc(), { condition_npc_id_at_least_one({12898, 12897}), condition_npc_last_damage_time_distance(12898, 12897, 8s) }); // Molten Boss - Berserker/Firestorm
	emplace_conditions(timing_last_hit_npc(), { condition_npc_id(12267) }); // Aetherblade - Frizz

	// Raids
	emplace_conditions(timing_last_hit_npc(), { condition_npc_id(15375) }); // Sabetha
	emplace_conditions(timing_last_hit_npc(), { condition_npc_id(16115) }); // Matthias Gabrel
	emplace_conditions(timing_last_hit_npc(), { condition_npc_id(17154) }); // Deimos
	emplace_conditions(timing_last_hit_npc(), { condition_npc_id(19450) }); // Dhuum
	emplace_conditions(timing_last_hit_npc(), { condition_npc_id(21041) }); // Qadim
	emplace_conditions(timing_last_hit_npc(), { condition_npc_id(22000) }); // Qadim the Peerless
	emplace_conditions(timing_last_hit_npc(), { condition_npc_id(16246), condition_npc_id(16286) }); // Xera

	// Dungeons
	emplace_conditions(timing_last_hit_npc(), { condition_npc_id(7018) });  // AC P1
	emplace_conditions(timing_last_hit_npc(), { condition_npc_id(9077) });  // AC P2
	emplace_conditions(timing_last_hit_npc(), { condition_npc_id(9078) });  // AC P3
	emplace_conditions(timing_last_hit_npc(), { condition_npc_id(10580) }); // Arah P2
	emplace_conditions(timing_last_hit_npc(), { condition_npc_id(10218) }); // CoE P1
	emplace_conditions(timing_last_hit_npc(), { condition_npc_id(10337) }); // CoE P2
	emplace_conditions(timing_last_hit_npc(), { condition_npc_id(10404) }); // CoE P3
}

void BossKillRecognition::emplace_conditions(std::function<std::chrono::system_clock::time_point(EncounterData&)> timing, std::initializer_list<std::function<bool(EncounterData&)>> initializer) {
	BossDescription boss(timing, initializer);
	bosses.emplace_back(boss);
}

std::function<bool(EncounterData&)> condition_npc_id(uintptr_t npc_id) {
	return [&, npc_id](EncounterData& data) {
		bool condition = data.log_species_id == npc_id;
		for (const auto &[id, agent] : data.log_agents) {
			condition |= agent.species_id == npc_id;
		}

		if (condition) {
			log_debug("timer: NPC ID Condition (" + std::to_string(npc_id) + ") returned true");
		}

		return condition;
	};
}

std::function<bool(EncounterData&)> condition_npc_damage_dealt(uintptr_t npc_id, long damage) {
	return [&, npc_id, damage](EncounterData& data) {
		bool condition = false;
		for (const auto& [id, agent] : data.log_agents) {
			if (agent.species_id == npc_id) {
				if (agent.damage_dealt > damage) {
					condition = true;
					break;
				}
			}
		}

		if (condition) {
			log_debug("timer: NPC Damage Dealt Condition (" + std::to_string(npc_id) + ", " + std::to_string(damage) + ") returned true");
		}

		return condition;
	};
}


std::function<bool(EncounterData&)> condition_npc_damage_taken(uintptr_t npc_id, long damage) {
	return [&, npc_id, damage](EncounterData& data) {
		bool condition = false;
		for (const auto& [id, agent] : data.log_agents) {
			if (agent.species_id == npc_id) {
				if (agent.damage_taken > damage) {
					condition = true;
					break;
				}
			}
		}

		if (condition) {
			log_debug("timer: NPC Damage Taken Condition (" + std::to_string(npc_id) + ", " + std::to_string(damage) + ") returned true");
		}

		return condition;
	};
}

std::function<bool(EncounterData&)> condition_npc_id_at_least_one(std::set<uintptr_t> npc_ids) {
	return [&, npc_ids](EncounterData& data) {
		bool condition = npc_ids.find(data.log_species_id) != npc_ids.end();
		for (const auto& [id, agent] : data.log_agents) {
			condition |= npc_ids.find(agent.species_id) != npc_ids.end();
		}

		if (condition) {
			std::string npc_ids_string = "";
			for (auto& id : npc_ids) {
				npc_ids_string += std::to_string(id) + ", ";
			}
			log_debug("timer: NPC ID At Least One Condition (" + npc_ids_string + ") returned true");
		}

		return condition;
	};
}

std::function<bool(EncounterData&)> condition_npc_last_damage_time_distance(uintptr_t npc_id_a, uintptr_t npc_id_b, std::chrono::system_clock::duration distance) {
	return [&, npc_id_a, npc_id_b, distance](EncounterData& data) {
		bool condition = true;

		bool a_found = false;
		std::chrono::system_clock::time_point last_hit_a;
		bool b_found = false;
		std::chrono::system_clock::time_point last_hit_b;

		for (const auto& [id, agent] : data.log_agents) {
			if (agent.species_id == npc_id_a) {
				if (!a_found || agent.last_hit > last_hit_a) {
					last_hit_a = agent.last_hit;
				}
				a_found = true;
			}

			if (agent.species_id == npc_id_b) {
				if (!b_found || agent.last_hit > last_hit_b) {
					last_hit_b = agent.last_hit;
				}
				b_found = true;
			}
		}

		if (a_found && b_found) {
			condition = std::chrono::abs(last_hit_a - last_hit_b) > distance;
		}

		if (condition) {
			log_debug("timer: NPC Last Damage Distance Condition (" + std::to_string(npc_id_a) + ", " + std::to_string(npc_id_b) + ", " + std::to_string(distance.count()) + ") returned true");
		}

		return condition;
	};
}

std::function<bool(EncounterData&)> condition_map_id(uint32_t map_id) {
	return [&, map_id](EncounterData& data) {
		bool condition = data.map_id == map_id;
		
		if (condition) {
			log_debug("timer: Map ID Condition (" + std::to_string(map_id) + ") returned true");
		}

		return condition;
	};
}

std::function<std::chrono::system_clock::time_point(EncounterData&)> timing_last_hit_npc() {
	return [&](EncounterData& data) {
		auto time = std::chrono::system_clock::time_point::min();

		for (const auto& [id, agent] : data.log_agents) {
			if (agent.last_hit > time) {
				time = agent.last_hit;
			}
		}

		return time;
	};
}

std::function<std::chrono::system_clock::time_point(EncounterData&)> timing_last_hit_npc_id(uintptr_t npc_id) {
	return [&, npc_id](EncounterData& data) {
		auto time = std::chrono::system_clock::time_point::min();

		for (const auto& [id, agent] : data.log_agents) {
			if (agent.species_id == npc_id && agent.last_hit > time) {
				time = agent.last_hit;
			}
		}

		return time;
	};
}

void EncounterData::archive() noexcept {
	log_agents.clear();
}

BossDescription::BossDescription(std::function<std::chrono::system_clock::time_point(EncounterData&)> timing, std::vector<std::function<bool(EncounterData&)>> conditions)
:	conditions(conditions),
	timing(timing) {
}
