const ota = require('zigbee-herdsman-converters/lib/ota');

module.exports = {
  zigbeeModel: ['Kommando_Nano'],
  model: 'Kommando_Nano',
  vendor: 'Kommando',
  description: 'Kommando touchscreen Zigbee panel',
  ota: ota.zigbeeOTA,
  fromZigbee: [],
  toZigbee: [],
  exposes: [],
  meta: {
    multiEndpoint: true,
  },
  endpoint: () => ({
    ep1: 1,
    ep2: 2,
    ep3: 3,
    ep4: 4,
    ep5: 5,
    ep6: 6,
  }),
};
