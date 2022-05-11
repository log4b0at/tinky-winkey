#pragma once

#include <string>
#include <vector>
#include <ctime>
#include <ostream>

class KeyboardInputItem {
public:
    enum Type {
        TEXT,
        KEY_NAME
    };

    std::string value;
    Type type;
	int count = 1;

    KeyboardInputItem(Type type, const std::string& value)
        : type(type)
        , value(value) {};
};

class KeyboardInputLog {
    

public:
	std::time_t time = 0;
    std::vector<KeyboardInputItem> items;

    KeyboardInputLog() { }

    void push(const KeyboardInputItem& item) {
		if (item.type == item.TEXT && !items.empty() && items.back().type == item.type)
			items.back().value += item.value;
		else if (item.type == item.KEY_NAME && !items.empty() && items.back().value == item.value && items.back().type == item.type)
			items.back().count++;
		else
			items.push_back(item);
	}

    bool empty() { return items.empty(); }
    void set_time(std::time_t t) { time = t; }

    void clear() { items.clear(); };

    size_t size()
    {
        size_t size = 0;
        for (const auto& item : items)
            size += item.value.size();
        return size;
    };
};

std::ostream& operator<<(std::ostream& out, const KeyboardInputLog& log)
{
    for (auto& item : log.items) {
        switch (item.type) {
        case KeyboardInputItem::Type::KEY_NAME:
            out << "(" << item.value << ")";
			if (item.count > 1)
				out << "*" << item.count;
            break;
        case KeyboardInputItem::Type::TEXT:
            out << "\"" << item.value << '"';
            break;
        }
		out << " ";
    }
    return out;
}