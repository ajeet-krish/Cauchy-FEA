import { useState, useMemo } from 'react';
import type { Material, MaterialPreset } from '../types';

const MATERIAL_PRESETS: MaterialPreset[] = [
  {
    name: 'Structural Steel',
    E: 200,
    nu: 0.30,
    rho: 7850,
    alpha: 12e-6,
    chipColor: '#8b949e',
  },
  {
    name: 'Aluminum 6061-T6',
    E: 69,
    nu: 0.33,
    rho: 2700,
    alpha: 23e-6,
    chipColor: '#58a6ff',
  },
  {
    name: 'Titanium Ti-6Al-4V',
    E: 114,
    nu: 0.34,
    rho: 4430,
    alpha: 8.6e-6,
    chipColor: '#d29922',
  },
  {
    name: 'Copper C11000',
    E: 117,
    nu: 0.34,
    rho: 8960,
    alpha: 16.5e-6,
    chipColor: '#f0883e',
  },
  {
    name: 'Carbon Fiber (UD)',
    E: 181,
    nu: 0.28,
    rho: 1600,
    alpha: -0.4e-6,
    chipColor: '#a371f7',
  },
];

interface MaterialLibraryProps {
  material: Material;
  onChange: (material: Material) => void;
}

function MaterialLibrary({ material, onChange }: MaterialLibraryProps) {
  const [customE, setCustomE] = useState('');
  const [customNu, setCustomNu] = useState('');
  const [customRho, setCustomRho] = useState('');

  const matchedPreset = useMemo(() => {
    const eGpa = material.E / 1e9;
    return MATERIAL_PRESETS.find(
      (p) =>
        Math.abs(p.E - eGpa) < 0.5 &&
        Math.abs(p.nu - material.nu) < 0.005 &&
        Math.abs(p.rho - material.rho) < 50,
    );
  }, [material]);

  const currentName = matchedPreset ? matchedPreset.name : 'Custom';
  const currentChip = matchedPreset ? matchedPreset.chipColor : '#6e7681';
  const eGpa = material.E / 1e9;

  const handlePresetChange = (presetName: string) => {
    const preset = MATERIAL_PRESETS.find((p) => p.name === presetName);
    if (preset) {
      onChange({
        E: preset.E * 1e9,
        nu: preset.nu,
        rho: preset.rho,
        t: material.t,
        alpha: preset.alpha,
      });
    } else {
      setCustomE('');
      setCustomNu('');
      setCustomRho('');
    }
  };

  const handleCustomField = (field: 'E' | 'nu' | 'rho', value: string) => {
    const num = parseFloat(value);
    if (Number.isNaN(num) || num <= 0) return;

    let updated: Material;
    if (field === 'E') {
      setCustomE(value);
      updated = { ...material, E: num * 1e9 };
    } else if (field === 'nu') {
      setCustomNu(value);
      updated = { ...material, nu: num };
    } else {
      setCustomRho(value);
      updated = { ...material, rho: num };
    }
    onChange(updated);
  };

  const handleThicknessChange = (value: string) => {
    const num = parseFloat(value);
    if (Number.isFinite(num) && num > 0) {
      onChange({ ...material, t: num });
    }
  };

  const handleAlphaChange = (value: string) => {
    const num = parseFloat(value);
    if (Number.isFinite(num)) {
      onChange({ ...material, alpha: num });
    }
  };

  return (
    <div>
      {/* Material info card */}
      <div className="material-card">
        <div className="material-card-header">
          <span className="material-chip" style={{ background: currentChip }} />
          <span className="material-card-name">{currentName}</span>
        </div>
        <div className="material-card-props">
          <span>E = {eGpa.toFixed(1)} GPa</span>
          <span>nu = {material.nu.toFixed(2)}</span>
          <span>{material.rho} kg/m3</span>
        </div>
      </div>

      {/* Preset selector */}
      <div className="form-group">
        <label>Preset</label>
        <select
          value={matchedPreset ? matchedPreset.name : 'custom'}
          onChange={(e) => handlePresetChange(e.target.value)}
        >
          {MATERIAL_PRESETS.map((p) => (
            <option key={p.name} value={p.name}>
              {p.name} (E={p.E} GPa)
            </option>
          ))}
          <option value="custom">Custom</option>
        </select>
      </div>

      {/* Elastic modulus */}
      <div className="form-row">
        <div className="form-group">
          <label>E (GPa)</label>
          <input
            type="number"
            step="1"
            min="0.1"
            value={customE || eGpa.toFixed(1)}
            onChange={(e) => handleCustomField('E', e.target.value)}
          />
        </div>
        <div className="form-group">
          <label>Poisson (nu)</label>
          <input
            type="number"
            step="0.01"
            min="0"
            max="0.5"
            value={customNu || material.nu.toFixed(2)}
            onChange={(e) => handleCustomField('nu', e.target.value)}
          />
        </div>
      </div>

      {/* Density and thickness */}
      <div className="form-row">
        <div className="form-group">
          <label>Density (kg/m3)</label>
          <input
            type="number"
            min="1"
            value={customRho || material.rho}
            onChange={(e) => handleCustomField('rho', e.target.value)}
          />
        </div>
        <div className="form-group">
          <label>Thickness (m)</label>
          <input
            type="number"
            step="0.001"
            min="0.0001"
            value={material.t}
            onChange={(e) => handleThicknessChange(e.target.value)}
          />
        </div>
      </div>

      {/* Thermal expansion */}
      <div className="form-group">
        <label>Thermal Exp. (1/K)</label>
        <input
          type="number"
          step="1e-6"
          value={(material.alpha ?? 0).toExponential(1)}
          onChange={(e) => handleAlphaChange(e.target.value)}
        />
      </div>
    </div>
  );
}

export default MaterialLibrary;
