#!/bin/sh

tmpfile1="/tmp/HeapResultField.txt"
tmpfile2="/tmp/HeapwalkFinalResultField.txt"
tmpfile3="/tmp/HeapwalkFinalOutputField.txt"

if [ -f "$tmpfile1" ]; then
    rm "$tmpfile1"
fi
if [ -f "$tmpfile2" ]; then
    rm "$tmpfile2"
fi
if [ -f "$tmpfile3" ]; then
    rm "$tmpfile3"
fi

sleep 300

log_file="/rdklogs/logs/Heapwalkrss_log.txt"

# Get the current date and time
current_date=$(date)

 retry_file="/tmp/Heapwalkcheckrss_retry_count"
 max_retries=5

  # Initialize retry count if the file doesn't exist
  if [ ! -f "$retry_file" ]; then
    echo 0 > "$retry_file"
  fi

# Echo the date and time
echo "Current date and time: $current_date" >> "$log_file"
RSSInterval=`dmcli eRT getv Device.WiFi.MemwrapTool.Memwraptool_RSS_check_Interval | grep "value:" |  cut -f2- -d: |  cut -f2- -d:`
RSSThreshold=`dmcli eRT getv Device.WiFi.MemwrapTool.Memwraptool_RSS_Threshold | grep "value:" |  cut -f2- -d: |  cut -f2- -d:`
HeapwalkDuration=`dmcli eRT getv Device.WiFi.MemwrapTool.Memwraptool_Heapwalk_Duration | grep "value:" |  cut -f2- -d: |  cut -f2- -d:`
HeapwalkInterval=`dmcli eRT getv Device.WiFi.MemwrapTool.Memwraptool_Heapwalk_Interval | grep "value:" |  cut -f2- -d: |  cut -f2- -d:`

echo "RSS Interval: $RSSInterval" >> "$log_file"
echo "RSS Threshold: $RSSThresholdl" >> "$log_file"
echo "Heapwalk Duration: $HeapwalkDuration" >> "$log_file"
echo "Heapwalk Interval: $HeapwalkInterval" >> "$log_file"


#to get the assoclist
for((i=1;i<=24;i++)); do
    numdevices=`dmcli eRT getv Device.WiFi.AccessPoint.$i.AssociatedDeviceNumberOfEntries | grep "value:" | cut -f2- -d:| cut -f2- -d:`
    echo "VAP INDEX $i : $numdevices" >> "$log_file"
done

#to check for onewifi process
onewifi_pid=$(ps | grep "/usr/bin/OneWifi -subsys eRT\." | grep -v grep | awk '{print $1}')

if [ -z "$onewifi_pid" ]; then
  echo "$(date '+%Y-%m-%d %H:%M:%S') Onewifi process not found" >> "$log_file"
  retry_count=$(cat "$retry_file")
  if [ "$retry_count" -lt "$max_retries" ]; then
    echo $((retry_count + 1)) > "$retry_file"
    nohup bash /usr/ccsp/wifi/Heapwalkcheckrss.sh &
  else
    echo "$(date '+%Y-%m-%d %H:%M:%S') Max retries reached. Exiting script." >> "$log_file"
    exit 1
  fi
  exit 1
fi

    memleakutil <<EOF> /tmp/HeapResultField.txt
$onewifi_pid
3
EOF
echo "$(date '+%Y-%m-%d %H:%M:%S') Overall HEAPWALK is marked as walked at starting" >> "$log_file"
sleep 900
STATUS_FILE="/proc/$onewifi_pid/status"
if [ ! -f "$STATUS_FILE" ]; then
  echo "$(date '+%Y-%m-%d %H:%M:%S') Process with PID $onewifi_pid does not exist." >> "$log_file"
  retry_count=$(cat "$retry_file")
  if [ "$retry_count" -lt "$max_retries" ]; then
    echo $((retry_count + 1)) > "$retry_file"
    nohup bash /usr/ccsp/wifi/Heapwalkcheckrss.sh &
  else
    echo "$(date '+%Y-%m-%d %H:%M:%S') Max retries reached. Exiting script." >> "$log_file"
    exit 1
  fi
  exit 1
fi
initial_vmrss=$(grep -i 'VmRSS' "$STATUS_FILE" | awk '{print $2}')
echo "$(date '+%Y-%m-%d %H:%M:%S') Initial RSS : $initial_vmrss" >> "$log_file"
while true; do
    sleep $RSSInterval
    echo "$(date '+%Y-%m-%d %H:%M:%S') Sleeping for $RSSInterval" >> "$log_file"
    onewifi_pid2=$(ps | grep "/usr/bin/OneWifi -subsys eRT\." | grep -v grep | awk '{print $1}')
if [ -z "$onewifi_pid2" ]; then
  echo "$(date '+%Y-%m-%d %H:%M:%S') Onewifi process not found" >> "$log_file"
  retry_count=$(cat "$retry_file")
  if [ "$retry_count" -lt "$max_retries" ]; then
    echo $((retry_count + 1)) > "$retry_file"
    nohup bash /usr/ccsp/wifi/Heapwalkcheckrss.sh &
  else
    echo "$(date '+%Y-%m-%d %H:%M:%S') Max retries reached. Exiting script." >> "$log_file"
    exit 1
  fi
  exit 1
fi
if [ $onewifi_pid != $onewifi_pid2 ]; then
  echo "$(date '+%Y-%m-%d %H:%M:%S') Onewifi pid changed. Exiting the script" >> "$log_file"
  retry_count=$(cat "$retry_file")
  if [ "$retry_count" -lt "$max_retries" ]; then
    echo $((retry_count + 1)) > "$retry_file"
    nohup bash /usr/ccsp/wifi/Heapwalkcheckrss.sh &
  else
    echo "$(date '+%Y-%m-%d %H:%M:%S') Max retries reached. Exiting script." >> "$log_file"
    exit 1
  fi
  exit 1
fi
STATUS_FILE="/proc/$onewifi_pid2/status"
if [ ! -f "$STATUS_FILE" ]; then
  echo "$(date '+%Y-%m-%d %H:%M:%S') Process with PID $onewifi_pid does not exist." >> "$log_file"
  retry_count=$(cat "$retry_file")
  if [ "$retry_count" -lt "$max_retries" ]; then
    echo $((retry_count + 1)) > "$retry_file"
    nohup bash /usr/ccsp/wifi/Heapwalkcheckrss.sh &
  else
    echo "$(date '+%Y-%m-%d %H:%M:%S') Max retries reached. Exiting script." >> "$log_file"
    exit 1
  fi
  exit 1
fi
    current_vmrss=$(grep -i 'VmRSS' "$STATUS_FILE" | awk '{print $2}')
    echo "$(date '+%Y-%m-%d %H:%M:%S') Current RSS : $current_vmrss" >> "$log_file"
    rss_diff=$((current_vmrss - initial_vmrss))
    echo "$(date '+%Y-%m-%d %H:%M:%S') RSS Diff: $rss_diff" >> "$log_file"
    if [ "$rss_diff" -gt "$RSShreshold" ]; then
       heapwalk_pid=$(ps | grep "/usr/ccsp/wifi/HeapwalkField.sh" | grep -v grep | awk '{print $1}')
       echo "$(date '+%Y-%m-%d %H:%M:%S') HeapwalkField.sh pid : $heapwalk_pid" >> "$log_file"
          if [ -z "$heapwalk_pid" ]; then
            nohup bash /usr/ccsp/wifi/HeapwalkField.sh "$HeapwalkDuration" "$HeapwalkInterval" &
            echo "$(date '+%Y-%m-%d %H:%M:%S') RSS increased. Running the other script." >> "$log_file"
            sleep "$HeapwalkDuration"
          else
            echo "$(date '+%Y-%m-%d %H:%M:%S') Process already running" >> "$log_file"
          fi
    else
       echo "$(date '+%Y-%m-%d %H:%M:%S') RSS is not increased. Continuing with the script" >> "$log_file"
    fi
    initial_vmrss=$current_vmrss
    echo "$(date '+%Y-%m-%d %H:%M:%S') Initial RSS : $initial_vmrss" >> "$log_file"
done