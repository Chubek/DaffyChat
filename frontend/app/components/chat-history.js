// Chat history download functionality
class ChatHistoryExporter {
  constructor() {
    this.messages = [];
  }
  
  addMessage(message) {
    this.messages.push({
      timestamp: new Date().toISOString(),
      author: message.author,
      text: message.text,
      type: message.type || 'message'
    });
  }
  
  exportAsJSON() {
    const data = {
      room: this.getRoomInfo(),
      exported_at: new Date().toISOString(),
      message_count: this.messages.length,
      messages: this.messages
    };
    
    const json = JSON.stringify(data, null, 2);
    const blob = new Blob([json], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    
    const a = document.createElement('a');
    a.href = url;
    a.download = `daffychat-${this.getRoomName()}-${Date.now()}.json`;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
  }
  
  getRoomInfo() {
    const params = new URLSearchParams(window.location.search);
    return {
      name: params.get('room') || 'unknown',
      url: window.location.href
    };
  }
  
  getRoomName() {
    const params = new URLSearchParams(window.location.search);
    return params.get('room') || 'unknown';
  }
  
  clear() {
    this.messages = [];
  }
  
  getMessages() {
    return this.messages;
  }
}

window.ChatHistoryExporter = ChatHistoryExporter;
