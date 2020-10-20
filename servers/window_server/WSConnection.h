#pragma once
#include <libg/Rect.h>
#include <libg/String.h>
#include <libipc/ClientConnection.h>
#include <libipc/Encoder.h>
#include <libipc/ServerConnection.h>
#include <malloc.h>

class GreetMessage : public Message {
public:
    GreetMessage() { }
    int id() const override { return 1; }
    int decoder_magic() const override { return 320; }
    EncodedMessage encode() const override
    {
        EncodedMessage buffer;
        Encoder::append(buffer, decoder_magic());
        Encoder::append(buffer, id());
        return buffer;
    }

private:
};

class GreetMessageReply : public Message {
public:
    GreetMessageReply(uint32_t connection_id)
        : m_connection_id(connection_id)
    {
    }
    int id() const override { return 2; }
    int decoder_magic() const override { return 320; }
    uint32_t connection_id() const { return m_connection_id; }
    EncodedMessage encode() const override
    {
        EncodedMessage buffer;
        Encoder::append(buffer, decoder_magic());
        Encoder::append(buffer, id());
        Encoder::append(buffer, m_connection_id);
        return buffer;
    }

private:
    uint32_t m_connection_id;
};

class CreateWindowMessage : public Message {
public:
    CreateWindowMessage(uint32_t width, uint32_t height, int buffer_id)
        : m_width(width)
        , m_height(height)
        , m_buffer_id(buffer_id)
    {
    }
    int id() const override { return 3; }
    int decoder_magic() const override { return 320; }
    uint32_t width() const { return m_width; }
    uint32_t height() const { return m_height; }
    int buffer_id() const { return m_buffer_id; }
    EncodedMessage encode() const override
    {
        EncodedMessage buffer;
        Encoder::append(buffer, decoder_magic());
        Encoder::append(buffer, id());
        Encoder::append(buffer, m_width);
        Encoder::append(buffer, m_height);
        Encoder::append(buffer, m_buffer_id);
        return buffer;
    }

private:
    uint32_t m_width;
    uint32_t m_height;
    int m_buffer_id;
};

class CreateWindowMessageReply : public Message {
public:
    CreateWindowMessageReply(uint32_t window_id)
        : m_window_id(window_id)
    {
    }
    int id() const override { return 4; }
    int decoder_magic() const override { return 320; }
    uint32_t window_id() const { return m_window_id; }
    EncodedMessage encode() const override
    {
        EncodedMessage buffer;
        Encoder::append(buffer, decoder_magic());
        Encoder::append(buffer, id());
        Encoder::append(buffer, m_window_id);
        return buffer;
    }

private:
    uint32_t m_window_id;
};

class SetBufferMessage : public Message {
public:
    SetBufferMessage(uint32_t window_id, int buffer_id)
        : m_window_id(window_id)
        , m_buffer_id(buffer_id)
    {
    }
    int id() const override { return 5; }
    int decoder_magic() const override { return 320; }
    uint32_t window_id() const { return m_window_id; }
    int buffer_id() const { return m_buffer_id; }
    EncodedMessage encode() const override
    {
        EncodedMessage buffer;
        Encoder::append(buffer, decoder_magic());
        Encoder::append(buffer, id());
        Encoder::append(buffer, m_window_id);
        Encoder::append(buffer, m_buffer_id);
        return buffer;
    }

private:
    uint32_t m_window_id;
    int m_buffer_id;
};

class SetBarStyleMessage : public Message {
public:
    SetBarStyleMessage(uint32_t window_id, uint32_t color, int style)
        : m_window_id(window_id)
        , m_color(color)
        , m_style(style)
    {
    }
    int id() const override { return 6; }
    int decoder_magic() const override { return 320; }
    uint32_t window_id() const { return m_window_id; }
    uint32_t color() const { return m_color; }
    int style() const { return m_style; }
    EncodedMessage encode() const override
    {
        EncodedMessage buffer;
        Encoder::append(buffer, decoder_magic());
        Encoder::append(buffer, id());
        Encoder::append(buffer, m_window_id);
        Encoder::append(buffer, m_color);
        Encoder::append(buffer, m_style);
        return buffer;
    }

private:
    uint32_t m_window_id;
    uint32_t m_color;
    int m_style;
};

class SetTitleMessage : public Message {
public:
    SetTitleMessage(uint32_t window_id, LG::String title)
        : m_window_id(window_id)
        , m_title(title)
    {
    }
    int id() const override { return 7; }
    int decoder_magic() const override { return 320; }
    uint32_t window_id() const { return m_window_id; }
    LG::String title() const { return m_title; }
    EncodedMessage encode() const override
    {
        EncodedMessage buffer;
        Encoder::append(buffer, decoder_magic());
        Encoder::append(buffer, id());
        Encoder::append(buffer, m_window_id);
        Encoder::append(buffer, m_title);
        return buffer;
    }

private:
    uint32_t m_window_id;
    LG::String m_title;
};

class AddBarMessage : public Message {
public:
    AddBarMessage(uint32_t window_id, uint32_t color, int style)
        : m_window_id(window_id)
        , m_color(color)
        , m_style(style)
    {
    }
    int id() const override { return 8; }
    int decoder_magic() const override { return 320; }
    uint32_t window_id() const { return m_window_id; }
    uint32_t color() const { return m_color; }
    int style() const { return m_style; }
    EncodedMessage encode() const override
    {
        EncodedMessage buffer;
        Encoder::append(buffer, decoder_magic());
        Encoder::append(buffer, id());
        Encoder::append(buffer, m_window_id);
        Encoder::append(buffer, m_color);
        Encoder::append(buffer, m_style);
        return buffer;
    }

private:
    uint32_t m_window_id;
    uint32_t m_color;
    int m_style;
};

class InvalidateMessage : public Message {
public:
    InvalidateMessage(uint32_t window_id, LG::Rect rect)
        : m_window_id(window_id)
        , m_rect(rect)
    {
    }
    int id() const override { return 9; }
    int decoder_magic() const override { return 320; }
    uint32_t window_id() const { return m_window_id; }
    LG::Rect rect() const { return m_rect; }
    EncodedMessage encode() const override
    {
        EncodedMessage buffer;
        Encoder::append(buffer, decoder_magic());
        Encoder::append(buffer, id());
        Encoder::append(buffer, m_window_id);
        Encoder::append(buffer, m_rect);
        return buffer;
    }

private:
    uint32_t m_window_id;
    LG::Rect m_rect;
};

class WindowServerDecoder : public MessageDecoder {
public:
    WindowServerDecoder() { }
    int magic() const { return 320; }
    UniquePtr<Message> decode(const char* buf, size_t size, size_t& decoded_msg_len) override
    {
        int msg_id, decoder_magic;
        size_t saved_dml = decoded_msg_len;
        Encoder::decode(buf, decoded_msg_len, decoder_magic);
        Encoder::decode(buf, decoded_msg_len, msg_id);
        if (magic() != decoder_magic) {
            decoded_msg_len = saved_dml;
            return nullptr;
        }
        uint32_t var_connection_id;
        uint32_t var_width;
        uint32_t var_height;
        int var_buffer_id;
        uint32_t var_window_id;
        uint32_t var_color;
        int var_style;
        LG::String var_title;
        LG::Rect var_rect;

        switch (msg_id) {
        case 1:
            return new GreetMessage();
        case 2:
            Encoder::decode(buf, decoded_msg_len, var_connection_id);
            return new GreetMessageReply(var_connection_id);
        case 3:
            Encoder::decode(buf, decoded_msg_len, var_width);
            Encoder::decode(buf, decoded_msg_len, var_height);
            Encoder::decode(buf, decoded_msg_len, var_buffer_id);
            return new CreateWindowMessage(var_width, var_height, var_buffer_id);
        case 4:
            Encoder::decode(buf, decoded_msg_len, var_window_id);
            return new CreateWindowMessageReply(var_window_id);
        case 5:
            Encoder::decode(buf, decoded_msg_len, var_window_id);
            Encoder::decode(buf, decoded_msg_len, var_buffer_id);
            return new SetBufferMessage(var_window_id, var_buffer_id);
        case 6:
            Encoder::decode(buf, decoded_msg_len, var_window_id);
            Encoder::decode(buf, decoded_msg_len, var_color);
            Encoder::decode(buf, decoded_msg_len, var_style);
            return new SetBarStyleMessage(var_window_id, var_color, var_style);
        case 7:
            Encoder::decode(buf, decoded_msg_len, var_window_id);
            Encoder::decode(buf, decoded_msg_len, var_title);
            return new SetTitleMessage(var_window_id, var_title);
        case 8:
            Encoder::decode(buf, decoded_msg_len, var_window_id);
            Encoder::decode(buf, decoded_msg_len, var_color);
            Encoder::decode(buf, decoded_msg_len, var_style);
            return new AddBarMessage(var_window_id, var_color, var_style);
        case 9:
            Encoder::decode(buf, decoded_msg_len, var_window_id);
            Encoder::decode(buf, decoded_msg_len, var_rect);
            return new InvalidateMessage(var_window_id, var_rect);
        default:
            decoded_msg_len = saved_dml;
            return nullptr;
        }
    }

    UniquePtr<Message> handle(const Message& msg) override
    {
        if (magic() != msg.decoder_magic()) {
            return nullptr;
        }

        switch (msg.id()) {
        case 1:
            return handle(static_cast<const GreetMessage&>(msg));
        case 3:
            return handle(static_cast<const CreateWindowMessage&>(msg));
        case 5:
            return handle(static_cast<const SetBufferMessage&>(msg));
        case 6:
            return handle(static_cast<const SetBarStyleMessage&>(msg));
        case 7:
            return handle(static_cast<const SetTitleMessage&>(msg));
        case 8:
            return handle(static_cast<const AddBarMessage&>(msg));
        case 9:
            return handle(static_cast<const InvalidateMessage&>(msg));
        default:
            return nullptr;
        }
    }

    virtual UniquePtr<Message> handle(const GreetMessage& msg) { return nullptr; }
    virtual UniquePtr<Message> handle(const CreateWindowMessage& msg) { return nullptr; }
    virtual UniquePtr<Message> handle(const SetBufferMessage& msg) { return nullptr; }
    virtual UniquePtr<Message> handle(const SetBarStyleMessage& msg) { return nullptr; }
    virtual UniquePtr<Message> handle(const SetTitleMessage& msg) { return nullptr; }
    virtual UniquePtr<Message> handle(const AddBarMessage& msg) { return nullptr; }
    virtual UniquePtr<Message> handle(const InvalidateMessage& msg) { return nullptr; }
};

class MouseMessage : public Message {
public:
    MouseMessage(int win_id, uint32_t x, uint32_t y)
        : m_win_id(win_id)
        , m_x(x)
        , m_y(y)
    {
    }
    int id() const override { return 1; }
    int decoder_magic() const override { return 737; }
    int win_id() const { return m_win_id; }
    uint32_t x() const { return m_x; }
    uint32_t y() const { return m_y; }
    EncodedMessage encode() const override
    {
        EncodedMessage buffer;
        Encoder::append(buffer, decoder_magic());
        Encoder::append(buffer, id());
        Encoder::append(buffer, m_win_id);
        Encoder::append(buffer, m_x);
        Encoder::append(buffer, m_y);
        return buffer;
    }

private:
    int m_win_id;
    uint32_t m_x;
    uint32_t m_y;
};

class DisplayMessage : public Message {
public:
    DisplayMessage(LG::Rect rect)
        : m_rect(rect)
    {
    }
    int id() const override { return 2; }
    int decoder_magic() const override { return 737; }
    LG::Rect rect() const { return m_rect; }
    EncodedMessage encode() const override
    {
        EncodedMessage buffer;
        Encoder::append(buffer, decoder_magic());
        Encoder::append(buffer, id());
        Encoder::append(buffer, m_rect);
        return buffer;
    }

private:
    LG::Rect m_rect;
};

class WindowClientDecoder : public MessageDecoder {
public:
    WindowClientDecoder() { }
    int magic() const { return 737; }
    UniquePtr<Message> decode(const char* buf, size_t size, size_t& decoded_msg_len) override
    {
        int msg_id, decoder_magic;
        size_t saved_dml = decoded_msg_len;
        Encoder::decode(buf, decoded_msg_len, decoder_magic);
        Encoder::decode(buf, decoded_msg_len, msg_id);
        if (magic() != decoder_magic) {
            decoded_msg_len = saved_dml;
            return nullptr;
        }
        int var_win_id;
        uint32_t var_x;
        uint32_t var_y;
        LG::Rect var_rect;

        switch (msg_id) {
        case 1:
            Encoder::decode(buf, decoded_msg_len, var_win_id);
            Encoder::decode(buf, decoded_msg_len, var_x);
            Encoder::decode(buf, decoded_msg_len, var_y);
            return new MouseMessage(var_win_id, var_x, var_y);
        case 2:
            Encoder::decode(buf, decoded_msg_len, var_rect);
            return new DisplayMessage(var_rect);
        default:
            decoded_msg_len = saved_dml;
            return nullptr;
        }
    }

    UniquePtr<Message> handle(const Message& msg) override
    {
        if (magic() != msg.decoder_magic()) {
            return nullptr;
        }

        switch (msg.id()) {
        case 1:
            return handle(static_cast<const MouseMessage&>(msg));
        case 2:
            return handle(static_cast<const DisplayMessage&>(msg));
        default:
            return nullptr;
        }
    }

    virtual UniquePtr<Message> handle(const MouseMessage& msg) { return nullptr; }
    virtual UniquePtr<Message> handle(const DisplayMessage& msg) { return nullptr; }
};
