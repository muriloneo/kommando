const m = require('zigbee-herdsman-converters/lib/modernExtend');
const exposes = require('zigbee-herdsman-converters/lib/exposes');
const e = exposes.presets;
const ea = exposes.access;

const UI_CLUSTER = 0xFC11;

/* ZCL attributes (must match firmware) */
const ATTR_PAYLOAD = 0x0000;
const ATTR_STATE = 0x0001;
const ATTR_BACKLIGHT = 0x0010;
const ATTR_SCR_TIMEOUT = 0x0011;
const ATTR_DIM_LEVEL = 0x0012;
const ATTR_NIGHT_MODE = 0x0013;
const ATTR_NIGHT_BL = 0x0014;
const ATTR_DEEP_SLEEP = 0x0015;
const ATTR_SLEEP_TIMEOUT = 0x0016;

const ZCL_CHARSTR = 0x42;

function normalizeRaw(raw) {
    if (typeof raw === 'string') return raw;
    if (raw && typeof raw === 'object') {
        if ('value' in raw) raw = raw.value;
        else if ('data' in raw) raw = raw.data;
    }
    if (Array.isArray(raw)) return Buffer.from(raw);
    return raw;
}

function decodeCharStr(raw) {
    raw = normalizeRaw(raw);
    if (typeof raw === 'string') return raw.replace(/\u0000+$/g, '').trim();
    if (!Buffer.isBuffer(raw) || raw.length === 0) return null;
    const len = raw[0];
    if (len <= 0) return '';
    return raw.slice(1, 1 + len).toString().replace(/\u0000+$/g, '').trim();
}

function encodeCharStr(str) {
    const s = String(str);
    const buf = Buffer.alloc(s.length + 1);
    buf.writeUInt8(s.length, 0);
    buf.write(s, 1);
    return buf;
}

function parseTileIndexFromUiCmd(uiCmd) {
    if (typeof uiCmd !== 'string') return undefined;
    const parts = uiCmd.split(':');
    if (parts[0] !== 'C' || !parts[1]) return undefined;
    const idx = parseInt(parts[1], 10);
    return Number.isInteger(idx) && idx >= 0 ? idx : undefined;
}

function tileFromEndpoint(ep) {
    if (!Number.isInteger(ep)) return null;
    const idx = ep - 1;
    return idx >= 0 && idx <= 5 ? idx : null;
}

function reportConfig(attribute) {
    return {attribute, minimumReportInterval: 0, maximumReportInterval: 300, reportableChange: 0};
}

const fzLocal = {
    tile_onoff: {
        cluster: 'genOnOff',
        type: ['commandOn', 'commandOff'],
        convert: (model, msg) => {
            const ep = msg?.endpoint?.ID;
            const tile = tileFromEndpoint(ep);
            if (tile === null) return undefined;

            const isOn = msg.type === 'commandOn';
            return {
                action: isOn ? 'on' : 'off',
                tile_index: tile,
                action_endpoint: ep,
                tile_action: `T:${tile}:${isOn ? 1 : 0}`,
                [`tile_${tile}_state`]: isOn ? 'ON' : 'OFF',
            };
        },
    },

    tile_dim: {
        cluster: 'genLevelCtrl',
        type: ['commandMoveToLevel', 'commandMoveToLevelWithOnOff'],
        convert: (model, msg, publish, options, meta) => {
            const ep = msg?.endpoint?.ID;
            const tile = tileFromEndpoint(ep);
            if (tile === null) return undefined;

            const zbLevel = msg?.data?.level ?? 0;
            const transition =
                (msg?.data?.transtime != null) ? msg.data.transtime :
                (msg?.data?.transitionTime != null) ? msg.data.transitionTime :
                (msg?.data?.transition_time != null) ? msg.data.transition_time :
                0;

            /* UI backlight signature from firmware: ep=1 + transition=7 */
            if (ep === 1 && transition === 7) {
                const bl = Math.min(255, Math.max(0, Math.round((zbLevel * 255) / 254)));
                if (meta?.logger) meta.logger.info(`BACKLIGHT ep=${ep} zb=${zbLevel} backlight=${bl}`);
                return {action: `backlight:${bl}`, backlight: bl, tile_action: `B:${bl}`};
            }

            /* TAP fallback signature: transition=0 + level 0/254 */
            if (transition === 0 && (zbLevel === 0 || zbLevel >= 254)) {
                const isOn = zbLevel >= 254;
                if (meta?.logger) {
                    meta.logger.info(`TAP_FALLBACK ep=${ep} tile=${tile} level=${zbLevel} transition=${transition} -> ${isOn ? 'on' : 'off'}`);
                }
                return {
                    action: isOn ? 'on' : 'off',
                    tile_index: tile,
                    action_endpoint: ep,
                    tile_action: `T:${tile}:${isOn ? 1 : 0}`,
                    [`tile_${tile}_state`]: isOn ? 'ON' : 'OFF',
                };
            }

            const percent = Math.min(100, Math.round((zbLevel * 100) / 254));
            if (meta?.logger) meta.logger.info(`DIM ep=${ep} tile=${tile} zb=${zbLevel} lvl=${percent}`);
            return {
                action: `dim:${tile}:${percent}`,
                tile_index: tile,
                tile_level: percent,
                tile_action: `D:${tile}:${percent}`,
                [`tile_${tile}_level`]: percent,
            };
        },
    },

    ui_attrs: {
        cluster: UI_CLUSTER,
        type: ['attributeReport', 'readResponse'],
        convert: (model, msg) => {
            const result = {};
            const data = msg?.data;
            if (!data) return undefined;

            const isReport = msg.type === 'attributeReport';

            for (const [attr, rawValue] of Object.entries(data)) {
                const id = Number(attr);
                const value = normalizeRaw(rawValue);

                switch (id) {
                case ATTR_PAYLOAD: {
                    const uiCmd = decodeCharStr(value);
                    if (uiCmd != null) {
                        result.ui_cmd = uiCmd;
                        const idx = parseTileIndexFromUiCmd(uiCmd);
                        if (idx !== undefined) result.tile_index = idx;
                    }
                    break;
                }
                case ATTR_STATE: {
                    const actionRaw = decodeCharStr(value);
                    if (!actionRaw) break;

                    result.tile_action = actionRaw;

                    /* READY always emitted so HA can re-sync on reboot */
                    if (actionRaw === 'READY') result.action = 'ready';

                    if (!isReport) break;

                    const parts = actionRaw.split(':');
                    if (parts[0] === 'T' && parts.length >= 3) {
                        const tile = parseInt(parts[1], 10);
                        const isOn = parts[2] === '1';
                        result.action = isOn ? 'on' : 'off';
                        result.tile_index = tile;
                        result.action_endpoint = Number.isInteger(tile) ? tile + 1 : undefined;
                        if (tile >= 0 && tile <= 5) result[`tile_${tile}_state`] = isOn ? 'ON' : 'OFF';
                    } else if (parts[0] === 'D' && parts.length >= 3) {
                        const tile = parseInt(parts[1], 10);
                        const lvl = Math.max(0, Math.min(100, parseInt(parts[2], 10)));
                        result.action = `dim:${tile}:${lvl}`;
                        result.tile_index = tile;
                        result.action_endpoint = Number.isInteger(tile) ? tile + 1 : undefined;
                        result.tile_level = lvl;
                        if (tile >= 0 && tile <= 5) result[`tile_${tile}_level`] = lvl;
                    } else if (parts[0] === 'H' && parts.length >= 2) {
                        const tile = parseInt(parts[1], 10);
                        result.action = `hold:${tile}`;
                        result.tile_index = tile;
                        result.action_endpoint = Number.isInteger(tile) ? tile + 1 : undefined;
                    } else if (parts[0] === 'B' && parts.length >= 2) {
                        const lvl = Math.max(0, Math.min(255, parseInt(parts[1], 10)));
                        result.action = `backlight:${lvl}`;
                        result.backlight = lvl;
                    }
                    break;
                }
                case ATTR_BACKLIGHT:    result.backlight = value; break;
                case ATTR_SCR_TIMEOUT:  result.screen_timeout = value; break;
                case ATTR_DIM_LEVEL:    result.dim_level = value; break;
                case ATTR_NIGHT_MODE:   result.night_mode = value !== 0; break;
                case ATTR_NIGHT_BL:     result.night_brightness = value; break;
                case ATTR_DEEP_SLEEP:   result.deep_sleep = value !== 0; break;
                case ATTR_SLEEP_TIMEOUT: result.sleep_timeout = value; break;
                }
            }

            return Object.keys(result).length ? result : undefined;
        },
    },
};

const tzLocal = {
    ui_cmd: {
        key: ['ui_cmd'],
        convertSet: async (entity, key, value) => {
            await entity.write(UI_CLUSTER, {
                [ATTR_PAYLOAD]: {value: encodeCharStr(value), type: ZCL_CHARSTR},
            }, {disableDefaultResponse: true});

            const state = {ui_cmd: String(value)};
            const parts = String(value).split(':');

            if (parts[0] === 'S' && parts.length >= 3) {
                const idx = parseInt(parts[1], 10);
                if (idx >= 0 && idx <= 5) state[`tile_${idx}_state`] = parts[2] === '1' ? 'ON' : 'OFF';
            } else if (parts[0] === 'D' && parts.length >= 4) {
                const idx = parseInt(parts[1], 10);
                if (idx >= 0 && idx <= 5) {
                    state[`tile_${idx}_dimmable`] = parts[2] === '1';
                    state[`tile_${idx}_level`] = Math.max(0, Math.min(100, parseInt(parts[3], 10)));
                }
            } else if (parts[0] === 'L' && parts.length >= 3) {
                const idx = parseInt(parts[1], 10);
                if (idx >= 0 && idx <= 5) state[`tile_${idx}_level`] = Math.max(0, Math.min(100, parseInt(parts[2], 10)));
            }

            const ti = parseTileIndexFromUiCmd(String(value));
            if (ti !== undefined) state.tile_index = ti;

            return {state};
        },
    },
};

const definition = {
    fingerprint: [
        {modelID: 'Kommando_Nano', manufacturerName: 'Kommando'},
        {modelID: 'Kommando_Nano', manufacturerName: 'Kommando\u0000'},
        {modelID: 'Kommando_Nano'},
        /* legacy fielded firmware variants */
        {modelID: 'UI_Panel_C6', manufacturerName: 'CustomESP32'},
        {modelID: 'UI_Panel_C6', manufacturerName: 'CustomESP32\u0000'},
        {modelID: 'UI_Panel_C6'},
        {modelID: 'UI_Panel_C6_R1', manufacturerName: 'CustomESP32'},
        {modelID: 'UI_Panel_C6_R1', manufacturerName: 'CustomESP32\u0000'},
        {modelID: 'UI_Panel_C6_R1'},
    ],

    zigbeeModel: ['Kommando_Nano', 'UI_PANEL_C6', 'UI_Panel_C6', 'UI_Panel_C6_R1'],
    model: 'Kommando_Nano',
    vendor: 'Kommando',
    description: 'ESP32-C6 Zigbee UI Panel',

    extend: [
        m.deviceEndpoints({
            endpoints: {tile0: 1, tile1: 2, tile2: 3, tile3: 4, tile4: 5, tile5: 6},
        }),

        /* panel config entities from custom cluster */
        m.numeric({name: 'backlight', cluster: UI_CLUSTER, attribute: ATTR_BACKLIGHT, access: 'ALL', valueMin: 0, valueMax: 255}),
        m.numeric({name: 'screen_timeout', cluster: UI_CLUSTER, attribute: ATTR_SCR_TIMEOUT, access: 'ALL'}),
        m.numeric({name: 'dim_level', cluster: UI_CLUSTER, attribute: ATTR_DIM_LEVEL, access: 'ALL', valueMin: 0, valueMax: 255}),
        m.numeric({name: 'night_brightness', cluster: UI_CLUSTER, attribute: ATTR_NIGHT_BL, access: 'ALL', valueMin: 0, valueMax: 255}),
        m.numeric({name: 'sleep_timeout', cluster: UI_CLUSTER, attribute: ATTR_SLEEP_TIMEOUT, access: 'ALL'}),
        m.binary({name: 'night_mode', cluster: UI_CLUSTER, attribute: ATTR_NIGHT_MODE, valueOn: [true, 1], valueOff: [false, 0], access: 'ALL'}),
        m.binary({name: 'deep_sleep', cluster: UI_CLUSTER, attribute: ATTR_DEEP_SLEEP, valueOn: [true, 1], valueOff: [false, 0], access: 'ALL'}),
    ],

    fromZigbee: [
        fzLocal.tile_onoff,
        fzLocal.tile_dim,
        fzLocal.ui_attrs,
    ],

    toZigbee: [
        tzLocal.ui_cmd,
    ],

    exposes: [
        e.text('action', ea.STATE).withDescription('Tile action: on/off/hold:N/dim:N:LEVEL/ready/backlight:LEVEL'),
        e.numeric('tile_index', ea.STATE).withValueMin(0).withValueMax(5),
        e.numeric('action_endpoint', ea.STATE),
        e.numeric('tile_level', ea.STATE).withValueMin(0).withValueMax(100),
        e.text('tile_action', ea.STATE),

        e.binary('tile_0_state', ea.STATE, 'ON', 'OFF'),
        e.binary('tile_1_state', ea.STATE, 'ON', 'OFF'),
        e.binary('tile_2_state', ea.STATE, 'ON', 'OFF'),
        e.binary('tile_3_state', ea.STATE, 'ON', 'OFF'),
        e.binary('tile_4_state', ea.STATE, 'ON', 'OFF'),
        e.binary('tile_5_state', ea.STATE, 'ON', 'OFF'),

        e.numeric('tile_0_level', ea.STATE).withValueMin(0).withValueMax(100),
        e.numeric('tile_1_level', ea.STATE).withValueMin(0).withValueMax(100),
        e.numeric('tile_2_level', ea.STATE).withValueMin(0).withValueMax(100),
        e.numeric('tile_3_level', ea.STATE).withValueMin(0).withValueMax(100),
        e.numeric('tile_4_level', ea.STATE).withValueMin(0).withValueMax(100),
        e.numeric('tile_5_level', ea.STATE).withValueMin(0).withValueMax(100),

        e.binary('tile_0_dimmable', ea.STATE, true, false),
        e.binary('tile_1_dimmable', ea.STATE, true, false),
        e.binary('tile_2_dimmable', ea.STATE, true, false),
        e.binary('tile_3_dimmable', ea.STATE, true, false),
        e.binary('tile_4_dimmable', ea.STATE, true, false),
        e.binary('tile_5_dimmable', ea.STATE, true, false),

        e.text('ui_cmd', ea.SET).withDescription('Panel command channel (C:/S:/D:/L:)'),
    ],

    configure: async (device, coordinatorEndpoint, logger) => {
        const ep1 = device.getEndpoint(1);
        if (!ep1) return;

        try {
            await ep1.bind('genBasic', coordinatorEndpoint);
        } catch (err) {
            if (logger?.warn) logger.warn(`genBasic bind failed: ${err}`);
        }

        try {
            await ep1.read('genBasic', ['manufacturerName', 'modelId']);
        } catch (err) {
            if (logger?.warn) logger.warn(`genBasic read failed: ${err}`);
        }

        try {
            await ep1.bind(UI_CLUSTER, coordinatorEndpoint);
        } catch (err) {
            if (logger?.warn) logger.warn(`UI_CLUSTER bind failed: ${err}`);
        }

        try {
            await ep1.configureReporting(UI_CLUSTER, [
                reportConfig(ATTR_STATE),
                reportConfig(ATTR_BACKLIGHT),
                reportConfig(ATTR_SCR_TIMEOUT),
                reportConfig(ATTR_DIM_LEVEL),
                reportConfig(ATTR_NIGHT_MODE),
                reportConfig(ATTR_NIGHT_BL),
                reportConfig(ATTR_DEEP_SLEEP),
                reportConfig(ATTR_SLEEP_TIMEOUT),
            ]);
        } catch (err) {
            if (logger?.warn) logger.warn(`UI_CLUSTER reporting config failed: ${err}`);
        }

        try {
            await ep1.read(UI_CLUSTER, [
                ATTR_BACKLIGHT,
                ATTR_SCR_TIMEOUT,
                ATTR_DIM_LEVEL,
                ATTR_NIGHT_MODE,
                ATTR_NIGHT_BL,
                ATTR_DEEP_SLEEP,
                ATTR_SLEEP_TIMEOUT,
            ]);
        } catch (err) {
            if (logger?.warn) logger.warn(`UI_CLUSTER initial read failed: ${err}`);
        }

        device.save();
    },
};

module.exports = [definition];
