var addon = require('addon');

function cuckoosync(name) {
    this.greet = function(str) {
        return _addonInstance.greet(str);
    }

    var _addonInstance = new addon.cuckoosync(name);
}

module.exports = cuckoosync;
