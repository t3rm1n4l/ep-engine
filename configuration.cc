/* -*- Mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 *     Copyright 2013 Zynga, Inc.
 *     Copyright 2011 Couchbase, Inc
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 */

#include "config.h"

#include "configuration.hh"
#include <vector>
#include <limits>
#include <sstream>
#include <iostream>

Configuration::Configuration() {
    initialize();
}

#include "generated_configuration.cc"

std::string Configuration::getString(const std::string &key) const {
    Mutex *ptr = const_cast<Mutex*> (&mutex);
    LockHolder lh(*ptr);

    std::map<std::string, value_t>::const_iterator iter;
    if ((iter = attributes.find(key)) == attributes.end()) {
        return std::string();
    }
    assert(iter->second.datatype == DT_STRING);
    if (iter->second.val.v_string) {
        return std::string(iter->second.val.v_string);
    }
    return std::string();
}

bool Configuration::getBool(const std::string &key) const {
    Mutex *ptr = const_cast<Mutex*> (&mutex);
    LockHolder lh(*ptr);

    std::map<std::string, value_t>::const_iterator iter;
    if ((iter = attributes.find(key)) == attributes.end()) {
        return false;
    }
    assert(iter->second.datatype == DT_BOOL);
    return iter->second.val.v_bool;
}

float Configuration::getFloat(const std::string &key) const {
    Mutex *ptr = const_cast<Mutex*> (&mutex);
    LockHolder lh(*ptr);

    std::map<std::string, value_t>::const_iterator iter;
    if ((iter = attributes.find(key)) == attributes.end()) {
        return 0;
    }
    assert(iter->second.datatype == DT_FLOAT);
    return iter->second.val.v_float;
}

size_t Configuration::getInteger(const std::string &key) const {
    Mutex *ptr = const_cast<Mutex*> (&mutex);
    LockHolder lh(*ptr);

    std::map<std::string, value_t>::const_iterator iter;
    if ((iter = attributes.find(key)) == attributes.end()) {
        return 0;
    }
    assert(iter->second.datatype == DT_SIZE);
    return iter->second.val.v_size;
}

std::ostream& operator <<(std::ostream &out, const Configuration &config) {
    LockHolder lh(const_cast<Mutex&> (config.mutex));
    std::map<std::string, Configuration::value_t>::const_iterator iter;
    for (iter = config.attributes.begin(); iter != config.attributes.end(); ++iter) {
        std::stringstream line;
        line << iter->first.c_str();
        line << " = [";
        switch (iter->second.datatype) {
        case DT_BOOL:
            if (iter->second.val.v_bool) {
                line << "true";
            } else {
                line << "false";
            }
            break;
        case DT_STRING:
            line << iter->second.val.v_string;
            break;
        case DT_SIZE:
            line << iter->second.val.v_size;
            break;
        case DT_FLOAT:
            line << iter->second.val.v_float;
            break;
        case DT_CONFIGFILE:
            continue;
        default:
            // ignore
            ;
        }
        line << "]" << std::endl;
        out << line.str();
    }

    return out;
}

void Configuration::setParameter(const std::string &key, bool value) {
    LockHolder lh(mutex);
    std::map<std::string, value_t>::iterator validator = attributes.find(key);
    if (validator != attributes.end()) {
        if (validator->second.validator != NULL) {
            if (!validator->second.validator->validateBool(key, value)) {
                throw std::runtime_error("value out of range.");
            }
        }
    }
    attributes[key].datatype = DT_BOOL;
    attributes[key].val.v_bool = value;
    std::vector<ValueChangedListener*> copy(attributes[key].changeListener);
    lh.unlock();
    std::vector<ValueChangedListener*>::iterator iter;
    for (iter = copy.begin(); iter != copy.end(); ++iter) {
        (*iter)->booleanValueChanged(key, value);
    }
}

void Configuration::setParameter(const std::string &key, size_t value) {
    LockHolder lh(mutex);
    std::map<std::string, value_t>::iterator validator = attributes.find(key);
    if (validator != attributes.end()) {
        if (validator->second.validator != NULL) {
            if (!validator->second.validator->validateSize(key, value)) {
                throw std::runtime_error("value out of range.");
            }
        }
    }
    attributes[key].datatype = DT_SIZE;
    if (key.compare("cache_size") == 0) {
        attributes["max_size"].val.v_size = value;
    } else {
        attributes[key].val.v_size = value;
    }

    std::vector<ValueChangedListener*> copy(attributes[key].changeListener);
    lh.unlock();
    std::vector<ValueChangedListener*>::iterator iter;
    for (iter = copy.begin(); iter != copy.end(); ++iter) {
        (*iter)->sizeValueChanged(key, value);
    }
}

void Configuration::setParameter(const std::string &key, float value) {
    LockHolder lh(mutex);

    std::map<std::string, value_t>::iterator validator = attributes.find(key);
    if (validator != attributes.end()) {
        if (validator->second.validator != NULL) {
            if (!validator->second.validator->validateFloat(key, value)) {
                throw std::runtime_error("value out of range.");
            }
        }
    }

    attributes[key].datatype = DT_FLOAT;
    attributes[key].val.v_float = value;
    std::vector<ValueChangedListener*> copy(attributes[key].changeListener);
    lh.unlock();
    std::vector<ValueChangedListener*>::iterator iter;
    for (iter = copy.begin(); iter != copy.end(); ++iter) {
        (*iter)->floatValueChanged(key, value);
    }
}

void Configuration::setParameter(const std::string &key, const std::string &value) {
    if (value.length() == 0) {
        setParameter(key, (const char *)NULL);
    } else {
        setParameter(key, value.c_str());
    }
}

void Configuration::setParameter(const std::string &key, const char *value) {
    LockHolder lh(mutex);
    std::map<std::string, value_t>::iterator validator = attributes.find(key);
    if (validator != attributes.end()) {
        if (validator->second.validator != NULL) {
            if (!validator->second.validator->validateString(key, value)) {
                throw std::runtime_error("value out of range.");
            }
        }
    }

    if (attributes.find(key) != attributes.end() && attributes[key].datatype
            == DT_STRING) {
        free((void*)attributes[key].val.v_string);
    }
    attributes[key].datatype = DT_STRING;
    attributes[key].val.v_string = NULL;
    if (value != NULL) {
        attributes[key].val.v_string = strdup(value);
    }

    std::vector<ValueChangedListener*> copy(attributes[key].changeListener);
    lh.unlock();
    std::vector<ValueChangedListener*>::iterator iter;
    for (iter = copy.begin(); iter != copy.end(); ++iter) {
        (*iter)->stringValueChanged(key, value);
    }
}

void Configuration::addValueChangedListener(const std::string &key,
                                            ValueChangedListener *val) {
    LockHolder lh(mutex);
    if (attributes.find(key) != attributes.end()) {
        attributes[key].changeListener.push_back(val);
    }
}

ValueChangedValidator *Configuration::setValueValidator(const std::string &key,
                                                        ValueChangedValidator *validator) {
    ValueChangedValidator *ret = 0;
    LockHolder lh(mutex);
    if (attributes.find(key) != attributes.end()) {
        ret = attributes[key].validator;
        attributes[key].validator = validator;
    }

    return ret;
}

void Configuration::addStats(ADD_STAT add_stat, const void *c) const {
    LockHolder lh(const_cast<Mutex&> (mutex));
    std::map<std::string, value_t>::const_iterator iter;
    for (iter = attributes.begin(); iter != attributes.end(); ++iter) {
        std::stringstream value;
        switch (iter->second.datatype) {
        case DT_BOOL:
            value << iter->second.val.v_bool;
            break;
        case DT_STRING:
            value << iter->second.val.v_string;
            break;
        case DT_SIZE:
            value << iter->second.val.v_size;
            break;
        case DT_FLOAT:
            value << iter->second.val.v_float;
            break;
        case DT_CONFIGFILE:
        default:
            // ignore
            ;
        }

        std::stringstream key;
        key << "ep_" << iter->first;
        std::string k = key.str();
        add_stat(k.c_str(),
                static_cast<uint16_t> (k.length()),
                value.str().data(),
                static_cast<uint32_t> (value.str().length()), c);
    }
}

/**
 * Internal container of an engine parameter.
 */
class ConfigItem: public config_item {
public:
    ConfigItem(const char *theKey, config_datatype theDatatype) {
        key = theKey;
        datatype = theDatatype;
        value.dt_string = &holder;
    }

private:
    char *holder;
};

bool Configuration::parseConfiguration(const char *str, SERVER_HANDLE_V1* sapi) {
    std::vector<ConfigItem *> config;

    std::map<std::string, value_t>::const_iterator iter;
    for (iter = attributes.begin(); iter != attributes.end(); ++iter) {
        config.push_back(new ConfigItem(iter->first.c_str(),
                                        iter->second.datatype));
    }

    // we don't have a good support for alias yet...
    config.push_back(new ConfigItem("cache_size", DT_SIZE));

    // And add support for config files...
    config.push_back(new ConfigItem("config_file", DT_CONFIGFILE));

    int nelem = config.size();
    struct config_item *items = (struct config_item*)calloc(nelem + 1,
            sizeof(struct config_item));
    for (int ii = 0; ii < nelem; ++ii) {
        items[ii].key = config[ii]->key;
        items[ii].datatype = config[ii]->datatype;
        items[ii].value.dt_string = config[ii]->value.dt_string;
    }
    items[nelem].key = NULL;

    bool ret = sapi->core->parse_config(str, items, stderr) == 0;
    for (int ii = 0; ii < nelem; ++ii) {
        if (items[ii].found) {
            if (ret) {
                switch (items[ii].datatype) {
                case DT_STRING:
                    setParameter(items[ii].key, *(items[ii].value.dt_string));
                    break;
                case DT_SIZE:
                    setParameter(items[ii].key, *items[ii].value.dt_size);
                    break;
                case DT_BOOL:
                    setParameter(items[ii].key, *items[ii].value.dt_bool);
                    break;
                case DT_FLOAT:
                    setParameter(items[ii].key, *items[ii].value.dt_float);
                    break;
                default:
                    abort();
                }
            }

            if (items[ii].datatype == DT_STRING) {
                free(*items[ii].value.dt_string);
            }
        }
    }

    // release allocated memory..
    free(items);
    std::vector<ConfigItem *>::iterator ii;
    for (ii = config.begin(); ii != config.end(); ++ii) {
        delete *ii;
    }

    return ret;
}

Configuration::~Configuration() {
    std::map<std::string, value_t>::iterator iter;
    for (iter = attributes.begin(); iter != attributes.end(); ++iter) {
        std::vector<ValueChangedListener*>::iterator ii;
        for (ii = iter->second.changeListener.begin();
             ii != iter->second.changeListener.end(); ++ii) {
            delete *ii;
        }

        delete iter->second.validator;
        if (iter->second.datatype == DT_STRING) {
            free((void*)iter->second.val.v_string);
        }
    }
}
