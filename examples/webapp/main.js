class Preview{

    constructor(element){
        this.element = element;
        
        this.peerconnection = new RTCPeerConnection();
    }
    
}

document.addEventListener("DOMContentLoaded", function() {
    const videoElement = document.getElementById("video");
    const configuration = {'iceServers': [{'urls': 'stun:stun.l.google.com:19302'}]};
    const peerconnection = new RTCPeerConnection(configuration);


    const websocket = new WebSocket("ws://localhost:9000/ws")
    websocket.onmessage = (message)=> {
        const messageObj = JSON.parse(message.data);
            console.log(messageObj.action)
        if ("action" in messageObj){
            console.log("ici");
            if (messageObj.action == "sdp"){
                console.log("SDP Action received");
                peerconnection.setRemoteDescription(new RTCSessionDescription(messageObj.params))
                .then( () => {
                    peerconnection.createAnswer().then( (answer) => {
                        websocket.send(JSON.stringify({"action": "sdp", params: answer}));
                        peerconnection.setLocalDescription(answer);
                    })
                }).catch( (e) => {
                    console.log(e)
                });
            }else if (messageObj.action == "ice"){
                peerconnection.addIceCandidate(new RTCIceCandidate(messageObj.params));
            }
        }else {
            console.log("Wrong message format");
        }
    }

    peerconnection.onicecandidate = (ev) => {
        websocket.send(JSON.stringify({"action": "ice", params: ev.candidate}));
    }

    peerconnection.ontrack = (track) => {
        videoElement.srcObject = track.streams[0];
        videoElement.play();

    }

    websocket.onopen = () => {
        console.log("send create stream");
        websocket.send('{"action": "play"}');
    }
});