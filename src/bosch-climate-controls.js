// Dashboard labels only. Service values come unchanged from the climate entity.
class BoschClimateControlsFeature extends HTMLElement {
  constructor() {
    super(); this.attachShadow({mode:'open'});
    this.shadowRoot.innerHTML = `<style>
      :host{display:block}.control{position:relative}.icon,.arrow{position:absolute;top:50%;transform:translateY(-50%);pointer-events:none;--mdc-icon-size:24px;color:var(--primary-text-color,#222)}.icon{left:14px}.arrow{right:12px;--mdc-icon-size:20px}select{appearance:none;-webkit-appearance:none;box-sizing:border-box;width:100%;height:var(--feature-height,42px);border:0;border-radius:var(--ha-card-border-radius,12px);padding:0 44px 0 52px;background:var(--secondary-background-color,#f2f2f2);color:var(--primary-text-color,#222);font:inherit;cursor:pointer}select:focus-visible{outline:2px solid var(--primary-color);outline-offset:2px}select:disabled{opacity:.5}.error{color:var(--error-color,#db4437);font-size:12px}.error:empty{display:none}
    </style><div class="control"><ha-icon class="icon" aria-hidden="true"></ha-icon><select></select><ha-icon class="arrow" icon="mdi:menu-down" aria-hidden="true"></ha-icon></div><div class="error" role="status"></div>`;
    this._select=this.shadowRoot.querySelector('select');
    this._icon=this.shadowRoot.querySelector('ha-icon');
    this._select.addEventListener('click',e=>e.stopPropagation());
    this._select.addEventListener('change',e=>{e.stopPropagation();this._change(e.target.value);});
  }
  setConfig(config) {
    if(!['fan_mode','preset_mode'].includes(config.control)) throw Error('control: fan_mode tai preset_mode vaaditaan');
    this._config={...config};this._update();
  }
  set hass(hass){this._hass=hass;this._update();}
  set context(context){this._context=context;this._update();}
  _entity(){return this._config?.climate_entity||this._context?.entity_id;}
  _state(){return this._hass?.states[this._entity()];}
  _label(value){
    const key=value.toLowerCase();
    if(this._config.control==='fan_mode') return ({auto:'Auto',silent:'1',quiet:'1',low:'2',medium:'3',mid:'3',high:'4',turbo:'5'})[key]||value;
    return ({none:'Ei mitään',boost:'Tehostus',eco:'ECO',sleep:'Yötila'})[key]||value;
  }
  _iconName(value){
    const key=(value||'').toLowerCase();
    if(this._config.control==='fan_mode') return ({auto:'mdi:fan-auto',silent:'mdi:fan-chevron-down',quiet:'mdi:fan-chevron-down',low:'mdi:fan-speed-1',medium:'mdi:fan-speed-2',mid:'mdi:fan-speed-2',high:'mdi:fan-speed-3',turbo:'mdi:fan-chevron-up'})[key]||'mdi:fan';
    return ({none:'mdi:circle-small',boost:'mdi:rocket-launch',eco:'mdi:leaf',sleep:'mdi:weather-night'})[key]||'mdi:tune';
  }
  _update(){
    if(!this._config)return;
    const state=this._state(),key=this._config.control,options=state?.attributes?.[key+'s']||[];
    this._select.replaceChildren();
    for(const value of options){const option=document.createElement('option');option.value=value;option.textContent=this._label(value);this._select.append(option);}
    this._select.value=state?.attributes?.[key]||'';
    this._icon.setAttribute('icon',this._iconName(this._select.value));
    this._select.setAttribute('aria-label',key==='fan_mode'?'Puhallinnopeus':'Esiasetus');
    this._select.disabled=!!this._busy||!state||['unavailable','unknown'].includes(state.state)||!options.length;
    this.shadowRoot.querySelector('.error').textContent=this._error||'';
  }
  async _change(value){
    const state=this._state(),key=this._config?.control;
    if(this._busy||!state||['unavailable','unknown'].includes(state.state)||!state.attributes[key+'s']?.includes(value))return;
    this._busy=true;this._error='';this._update();
    try{await this._hass.callService('climate','set_'+key,{entity_id:this._entity(),[key]:value});}
    catch(error){this._error=error?.message||String(error);}
    finally{this._busy=false;this._update();}
  }
}
if(!customElements.get('bosch-climate-controls-feature'))customElements.define('bosch-climate-controls-feature',BoschClimateControlsFeature);
window.customCardFeatures=window.customCardFeatures||[];
if(!window.customCardFeatures.some(x=>x.type==='bosch-climate-controls-feature'))window.customCardFeatures.push({type:'bosch-climate-controls-feature',name:'Bosch ilmastoinnin valinnat',configurable:false,isSupported:(hass,context)=>context?.entity_id?.startsWith('climate.')||false});
