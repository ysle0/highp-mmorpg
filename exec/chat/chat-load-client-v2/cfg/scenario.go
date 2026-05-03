package cfg

import (
	"errors"
	"fmt"
	"sort"
	"strconv"
	"strings"

	"github.com/BurntSushi/toml"
)

var (
	// errPhaseNotFound is returned when a phase of a specific type is not found.
	errPhaseNotFound = errors.New("phase not found")
)

type ScenarioConfig struct {
	Name           string
	Description    string
	TargetHost     string
	TargetPort     int
	ClientCount    int
	NicknamePrefix string
	Phases         []*PhaseConfig
}

// scenarioFile represents the top-level structure of the TOML configuration file.
type scenarioFile struct {
	Scenario struct {
		Name        string `toml:"name"`
		Description string `toml:"description"`
		TargetHost  string `toml:"target_host"`
		TargetPort  int    `toml:"target_port"`
	} `toml:"scenario"`
	Clients struct {
		Count          int    `toml:"count"`
		NicknamePrefix string `toml:"nickname_prefix"`
	} `toml:"clients"`
}

func NewWithFilepath(filepath string) (*ScenarioConfig, error) {
	var file scenarioFile
	if _, err := toml.DecodeFile(filepath, &file); err != nil {
		return nil, fmt.Errorf("decode scenario file %q: %w", filepath, err)
	}

	var raw map[string]toml.Primitive
	meta, err := toml.DecodeFile(filepath, &raw)
	if err != nil {
		return nil, fmt.Errorf("decode scenario file tables %q: %w", filepath, err)
	}

	phases, err := decodePhases(raw, meta)
	if err != nil {
		return nil, err
	}

	return &ScenarioConfig{
		Name:           file.Scenario.Name,
		Description:    file.Scenario.Description,
		TargetHost:     file.Scenario.TargetHost,
		TargetPort:     file.Scenario.TargetPort,
		ClientCount:    file.Clients.Count,
		NicknamePrefix: file.Clients.NicknamePrefix,
		Phases:         phases,
	}, nil
}

func decodePhases(raw map[string]toml.Primitive, meta toml.MetaData) ([]*PhaseConfig, error) {
	phaseByIndex := make(map[int]*PhaseConfig)

	for key, value := range raw {
		if !strings.HasPrefix(key, "phase_") {
			continue
		}

		indexText := strings.TrimPrefix(key, "phase_")
		index, err := strconv.Atoi(indexText)
		if err != nil || index < 0 {
			return nil, fmt.Errorf("invalid phase table name %q", key)
		}

		var phase PhaseConfig
		if err := meta.PrimitiveDecode(value, &phase); err != nil {
			return nil, fmt.Errorf("decode %s: %w", key, err)
		}

		phaseByIndex[index] = &phase
	}

	if len(phaseByIndex) == 0 {
		return nil, errors.New("scenario must define at least one phase_N table")
	}

	indexes := make([]int, 0, len(phaseByIndex))
	for index := range phaseByIndex {
		indexes = append(indexes, index)
	}
	sort.Ints(indexes)

	for expected, actual := range indexes {
		if expected != actual {
			return nil, fmt.Errorf("missing phase_%d before phase_%d", expected, actual)
		}
	}

	phases := make([]*PhaseConfig, 0, len(indexes))
	for _, index := range indexes {
		phases = append(phases, phaseByIndex[index])
	}
	return phases, nil
}

func (sc *ScenarioConfig) FindPhase(phaseType PhaseType) (*PhaseConfig, error) {
	for _, p := range sc.Phases {
		if p.Type == phaseType {
			return p, nil
		}
	}
	return nil, fmt.Errorf("%w: %s", errPhaseNotFound, phaseType)
}
