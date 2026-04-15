module message_highlighter
version 1.0.0

import dfc.bridge
import dfc.types { Color }

-- Highlight a message in the frontend with a given color
pub fn highlight(message_id: str, color: Color) {
    emit "highlight-message" {
        message_id: message_id,
        color: color.to_hex(),
    }
}

on hook "message-selected" (payload: {id: str}) {
    highlight(payload.id, Color.Yellow)
}

expect hook "highlight-message"

exports {
    highlight,
    on hook "message-selected",
}
