// Local Home Assistant card feature. No external dependencies.
const positions = [
  ["Swing", "M12 6l7 7 M8 13l10 4 M6 20h12 M8 27l10-4 M12 34l7-7"],
  ["Ylös", "M12 6l7 7"], ["Yläviistoon", "M8 13l10 4"],
  ["Keskelle", "M6 20h12"], ["Alaviistoon", "M8 27l10-4"],
  ["Alas", "M12 34l7-7"],
];
class BoschVaneFeature extends HTMLElement {
  constructor() {
    super();
    this.attachShadow({ mode: "open" });
    this.shadowRoot.innerHTML = `<style>
      :host { display:block; }
      .row { display:flex; height:var(--feature-height, 42px); padding:0;
        border-radius:var(--feature-border-radius,12px); overflow:hidden;
        background:var(--secondary-background-color,#f2f2f2); }
      button { flex:1; min-width:0; border:0; border-radius:var(--feature-border-radius,12px);
        background:transparent; color:var(--primary-text-color,#202124); cursor:pointer;
        display:grid; place-items:center; padding:0; }
      button[aria-pressed="true"] { background:var(--primary-color,#03a9f4); color:white; }
      button:hover:not(:disabled) { box-shadow:inset 0 0 0 2px var(--primary-color,#03a9f4); }
      button:focus-visible { outline:2px solid var(--primary-text-color,#202124); outline-offset:-3px; }
      button:disabled { opacity:.4; cursor:default; }
      svg { width:36px; height:36px; }
      .message { font:12px sans-serif; color:var(--error-color,#db4437); }
      .message:empty { display:none; }
    </style><div class="row" role="group" aria-label="Puhallussuunnan pyyntö"></div>
    <div class="message" role="status" aria-live="polite"></div>`;
    const row = this.shadowRoot.querySelector('.row');
    this._buttons = positions.map(([option, ray]) => {
      const button = document.createElement('button');
      button.type = 'button';
      button.title = option;
      button.setAttribute('aria-label', option);
      // Remote-style fixed housing with only the selected direction ray lit.
      button.innerHTML = `<svg viewBox="0 0 40 40" aria-hidden="true" fill="none"
        stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round">
        <path d="M24 7h10v26H24z"/>
        <path d="${ray}"/>
      </svg>`;
      button.addEventListener('click', (event) => {
        event.stopPropagation();
        this._select(option);
      });
      row.append(button);
      return button;
    });
  }
  setConfig(config) {
    if (!config || typeof config.entity !== 'string' || !config.entity.startsWith('select.'))
      throw new Error('Määritä entity: select.… (Bosch puhallussuunta pyyntö).');
    this._config = { ...config };
    this._update();
  }
  set hass(value) { this._hass = value; this._update(); }
  set context(value) { this._context = value; this._update(); }
  _climateId() { return this._config?.climate_entity || this._context?.entity_id; }
  _swingActive() {
    return ['vertical', 'both'].includes(this._hass?.states[this._climateId()]?.attributes?.swing_mode);
  }
  _update() {
    const state = this._hass?.states[this._config?.entity];
    const available = state && !['unavailable','unknown'].includes(state.state);
    const options = state?.attributes?.options || [];
    const climate = this._hass?.states[this._climateId()];
    const ready = climate && !['unavailable', 'unknown'].includes(climate.state);
    const modes = climate?.attributes?.swing_modes || [];
    const swing = this._swingActive();
    this._buttons.forEach((button, i) => {
      const option = positions[i][0];
      button.disabled = !!this._busy || !ready || (option === 'Swing'
        ? !modes.includes(swing ? 'off' : 'vertical')
        : !available || !options.includes(option) || (swing && !modes.includes('off')));
      button.setAttribute('aria-pressed', String(option === 'Swing' ? swing : !swing && state?.state === option));
    });
    this.shadowRoot.querySelector('.message').textContent = this._error ||
      (this._hass && this._config && !state ? 'Valitsinta ei löytynyt: tarkista entiteettitunnus.' : '');
  }
  async _select(option) {
    const state = this._hass?.states[this._config?.entity];
    const button = this._buttons[positions.findIndex(([label]) => label === option)];
    if (!button || button.disabled || this._busy) return;
    this._busy = true;
    this._error = '';
    this._update();
    try {
      if (option === 'Swing') {
        await this._hass.callService('climate', 'set_swing_mode', {
          entity_id: this._climateId(), swing_mode: this._swingActive() ? 'off' : 'vertical',
        });
      } else {
        if (this._swingActive()) {
          await this._hass.callService('climate', 'set_swing_mode', {
            entity_id: this._climateId(), swing_mode: 'off',
          });
        }
        await this._hass.callService('select', 'select_option', {
          entity_id: this._config.entity, option,
        });
      }
    } catch (error) {
      this._error = `Pyyntö epäonnistui: ${error?.message || String(error)}`;
    } finally {
      this._busy = false;
      this._update();
    }
  }
}
if (!customElements.get('bosch-vane-feature')) customElements.define('bosch-vane-feature', BoschVaneFeature);
window.customCardFeatures = window.customCardFeatures || [];
if (!window.customCardFeatures.some(x => x.type === 'bosch-vane-feature')) {
  window.customCardFeatures.push({
    type: 'bosch-vane-feature', name: 'Bosch puhallussuunta', configurable: false,
    isSupported: (hass, context) => context?.entity_id?.startsWith('climate.') || false,
  });
}
