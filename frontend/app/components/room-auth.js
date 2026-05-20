// Room authentication component
class RoomAuth {
  constructor() {
    this.token = localStorage.getItem('daffy-room-token');
  }
  
  async promptPassword(roomName) {
    return new Promise((resolve, reject) => {
      const dialog = document.createElement('dialog');
      dialog.innerHTML = `
        <article style="max-width: 400px;">
          <header>
            <h3>Password Required</h3>
            <p style="color: var(--daffy-ink-muted); font-size: 0.9rem;">
              Room "${roomName}" is password protected
            </p>
          </header>
          <form id="password-form">
            <label for="room-password">Password</label>
            <input 
              type="password" 
              id="room-password" 
              placeholder="Enter room password"
              required
              autofocus
            >
            <div style="display: flex; gap: 1rem; margin-top: 1.5rem;">
              <button type="submit" class="daffy-btn-primary">
                <i data-lucide="unlock"></i> Unlock
              </button>
              <button type="button" class="daffy-btn-secondary" id="cancel-btn">
                Cancel
              </button>
            </div>
            <div id="error-msg" style="display: none; margin-top: 1rem; color: #dc2626; font-size: 0.85rem;"></div>
          </form>
        </article>
      `;
      
      document.body.appendChild(dialog);
      dialog.showModal();
      
      // Initialize Lucide icons
      if (window.lucide) {
        lucide.createIcons();
      }
      
      const form = dialog.querySelector('#password-form');
      const passwordInput = dialog.querySelector('#room-password');
      const errorMsg = dialog.querySelector('#error-msg');
      const cancelBtn = dialog.querySelector('#cancel-btn');
      
      form.addEventListener('submit', async (e) => {
        e.preventDefault();
        const password = passwordInput.value;
        
        try {
          const response = await this.authenticate(roomName, password);
          if (response.success) {
            this.token = response.token;
            localStorage.setItem('daffy-room-token', response.token);
            dialog.close();
            document.body.removeChild(dialog);
            resolve(response.token);
          } else {
            errorMsg.textContent = response.error || 'Invalid password';
            errorMsg.style.display = 'block';
            passwordInput.value = '';
            passwordInput.focus();
          }
        } catch (err) {
          errorMsg.textContent = 'Authentication failed';
          errorMsg.style.display = 'block';
        }
      });
      
      cancelBtn.addEventListener('click', () => {
        dialog.close();
        document.body.removeChild(dialog);
        reject(new Error('Authentication cancelled'));
      });
    });
  }
  
  async authenticate(roomName, password) {
    // In production: POST to /api/rooms/authenticate
    const response = await fetch('/api/rooms/authenticate', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ custom_name: roomName, password })
    });
    
    return await response.json();
  }
  
  getToken() {
    return this.token;
  }
  
  clearToken() {
    this.token = null;
    localStorage.removeItem('daffy-room-token');
  }
}

// Export for use in room.html
window.RoomAuth = RoomAuth;
