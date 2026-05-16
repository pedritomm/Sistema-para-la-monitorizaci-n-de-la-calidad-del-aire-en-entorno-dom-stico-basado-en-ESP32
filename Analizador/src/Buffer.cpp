#include <Buffer.h>

Buffer::Buffer(uint8_t max_size) // Constructor 
{
     m_max_size = max_size;
}
Buffer::~Buffer() // Destructor
{
    // Los datos que no sirvan el propio compilar lo hace sin necesidad del destructor
}

bool Buffer::isReceptionEmpty() // Dice si la cola de recepcion esta vacia (1/0)
{
    /*
    m_reception_mutex.lock(); // Si no lo he adquirido se queda aqui pillado
    // m_reception.size(): Obtengo el tamaño de la cola
    bool empty = (m_reception.size()==0); // Guarda en la variable empty si la cola esta vacia.
    m_reception_mutex.unlock();
    //return m_reception.size() == 0;  // Devuelve si la cola esta vacia.
    */
    std::lock_guard<std::mutex> lock(m_reception_mutex); // En cuanto sales de la funcion se auto desbloquea
    return (m_reception.size() == 0);
}
bool Buffer::isSendEmpty() // Dice si la cola de recepcion esta vacia (1/0)
{
    std::lock_guard<std::mutex> lock(m_send_mutex);
    return (m_send.size() == 0);
}
Buffer::Message Buffer::getReceivedMessage()
{
    std::lock_guard<std::mutex> lock(m_reception_mutex);
    Buffer::Message msg = m_reception.front(); //Obtienen el mensaje de la cola de recepcion
    m_reception.pop(); // Elimina el mensaje

    return msg;
}
bool Buffer::setReceivedMessage( Buffer::Message msg, bool remove_oldest)
{
    std::lock_guard<std::mutex> lock(m_reception_mutex);
    if(m_reception.size()< m_max_size)
    {
        m_reception.push(msg);
        return true;
    }
    else
    {
        if(remove_oldest)
        {
            m_reception.pop();
            m_reception.push(msg);
            return true;
        }
        else
        {
            return false;
        }
    }
    return true;
}
bool Buffer::setMessageToSend(Buffer::Message msg, bool remove_oldest)
{
    std::lock_guard<std::mutex> lock(m_send_mutex);
    if(m_send.size()< m_max_size)
    {
        m_send.push(msg);
        return true;
    }
    else
    {
        if(remove_oldest)
        {
            m_send.pop();
            m_send.push(msg);
            return true;
        }
        else
        {
            return false;
        }
    }
    return true;
}
Buffer::Message Buffer::getMessageToSend()
{
    std::lock_guard<std::mutex> lock(m_send_mutex);
    Buffer::Message msg = m_send.front(); //Obtienen el mensaje de la cola de envio
    m_send.pop(); // Elimina el mensaje
    
    return msg;
}
